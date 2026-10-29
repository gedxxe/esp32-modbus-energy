# Export only public trust material referenced by one existing Windows Wi-Fi profile.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$ProfileName,
    [string]$OutputDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'output/wifi-trust')
)
$ErrorActionPreference = 'Stop'
$destination = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $destination -Force | Out-Null
$temporary = Join-Path $destination ('profile-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary | Out-Null
# Never add key=clear. This command exports the profile without revealing its stored key.
Push-Location -LiteralPath $temporary
try {
    & netsh wlan export profile "name=$ProfileName" 'folder=.' 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Windows could not export that Wi-Fi profile. Check its name with netsh wlan show profiles.' }
}
finally { Pop-Location }
$xmlFiles = @(Get-ChildItem -LiteralPath $temporary -File -Filter '*.xml')
if ($xmlFiles.Count -ne 1) { throw 'Expected exactly one exported profile. No certificate was selected.' }
$profileXmlPath = $xmlFiles[0].FullName
try {
    $xml = [xml][IO.File]::ReadAllText($profileXmlPath)
    $value = { param($name) @($xml.SelectNodes("//*[local-name()='$name']")) | ForEach-Object { $_.InnerText.Trim() } }
    $trusted = @(& $value 'TrustedRootCA' | ForEach-Object { ($_ -replace '[^0-9a-fA-F]','').ToUpperInvariant() } | Select-Object -Unique)
    $serverNames = @(& $value 'ServerNames' | Where-Object { $_ })
    $validation = @(& $value 'PerformServerValidation')
    $eapTypes = @($xml.SelectNodes("//*[local-name()='Eap']/*[local-name()='Type']") | ForEach-Object { $_.InnerText })
    $report = [ordered]@{
        profile = $ProfileName
        authentication = @(& $value 'authentication')
        encryption = @(& $value 'encryption')
        eapTypes = $eapTypes
        performServerValidation = $validation
        serverNames = $serverNames
        trustedRootSHA1 = $trusted
        certificates = @()
        containsPasswordsOrPrivateKeys = $false
    }
    $allCertificates = @(Get-ChildItem Cert:\CurrentUser,Cert:\LocalMachine -Recurse -ErrorAction SilentlyContinue | Where-Object { $_ -is [Security.Cryptography.X509Certificates.X509Certificate2] })
    foreach ($thumbprint in $trusted) {
        if ($thumbprint -notmatch '^[0-9A-F]{40}$') { throw 'The profile has an unsupported CA reference. No certificate was guessed.' }
        $certificate = $allCertificates | Where-Object { $_.Thumbprint -eq $thumbprint } | Select-Object -First 1
        if ($null -eq $certificate) {
            $report.certificates += [ordered]@{ sha1=$thumbprint; found=$false; pem=$null }
            continue
        }
        $constraints = $certificate.Extensions | Where-Object { $_.Oid.Value -eq '2.5.29.19' } | Select-Object -First 1
        if ($null -eq $constraints -or -not $constraints.CertificateAuthority) { throw 'The referenced certificate is not a CA. No leaf certificate was exported.' }
        $pemPath = Join-Path $destination ($thumbprint.ToLowerInvariant() + '.pem')
        $base64 = [Convert]::ToBase64String($certificate.RawData,[Base64FormattingOptions]::InsertLineBreaks)
        [IO.File]::WriteAllText($pemPath,"-----BEGIN CERTIFICATE-----`n$base64`n-----END CERTIFICATE-----`n",[Text.Encoding]::ASCII)
        $report.certificates += [ordered]@{ sha1=$thumbprint; found=$true; pem=[IO.Path]::GetFileName($pemPath); subject=$certificate.Subject; expires=$certificate.NotAfter.ToUniversalTime().ToString('o') }
    }
    $reportPath = Join-Path $destination 'report.json'
    [IO.File]::WriteAllText($reportPath,($report | ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    Write-Output "Public profile report: $reportPath"
    if (-not $trusted.Count -or @($report.certificates | Where-Object { -not $_.found }).Count) {
        Write-Output 'The matching CA certificate is unavailable in the accessible Windows stores. The report contains its public reference only. No usable CA file was invented.'
        exit 2
    }
    Write-Output 'Public CA export complete. Check report.json and use the matching PEM in Advanced Wi-Fi settings. No password or private key was exported.'
}
finally {
    # Remove only the exact temporary XML created above, then its empty directory.
    Remove-Item -LiteralPath $profileXmlPath -Force
    Remove-Item -LiteralPath $temporary
}
