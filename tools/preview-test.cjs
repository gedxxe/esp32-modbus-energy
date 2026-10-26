// Exercise the compressed assets embedded in the firmware, using simulated APIs.
const fs = require("node:fs");
const path = require("node:path");
const http = require("node:http");
const zlib = require("node:zlib");
const crypto = require("node:crypto");
const assert = require("node:assert/strict");
const { chromium } = require(process.argv[2] || "playwright");
const root = path.resolve(__dirname, "..");
const version = fs
  .readFileSync(path.join(root, "firmware/include/version.h"), "utf8")
  .match(/FIRMWARE_VERSION\[\] = "([^"]+)"/)[1];
// Keep simulated labels/screenshots aligned with the installed board configuration.
const rgbPin = Number(fs.readFileSync(path.join(root, "firmware/include/board.h"), "utf8").match(/RGB_LED_PIN\s*=\s*(\d+)/)[1]);
const manifest = JSON.parse(
  fs.readFileSync(path.join(root, "web/bundle.json")),
);
const header = fs.readFileSync(
  path.join(root, "firmware/include/web_ui.h"),
  "utf8",
);
const embedded = Object.fromEntries(
  [
    ...header.matchAll(
      /const uint8_t (WEB_[A-Z_]+)\[\] PROGMEM = \{([\s\S]*?)\};/g,
    ),
  ].map(([, name, bytes]) => [
    name,
    Buffer.from(
      [...bytes.matchAll(/0x([a-f0-9]{2})/g)].map((match) =>
        parseInt(match[1], 16),
      ),
    ),
  ]),
);
for (const asset of Object.values(manifest.assets)) {
  assert.equal(embedded[asset.symbol].length, asset.gzip);
  assert.equal(
    crypto.createHash("sha256").update(embedded[asset.symbol]).digest("hex"),
    asset.sha256,
  );
  assert.equal(zlib.gunzipSync(embedded[asset.symbol]).length, asset.raw);
}
const upstream = JSON.parse(
  fs.readFileSync(path.join(root, "web/vendor/react-bits/upstream.json")),
);
for (const [file, sha] of Object.entries(upstream.files)) {
  const source = Buffer.from(
    fs
      .readFileSync(path.join(root, "web/vendor/react-bits", file), "utf8")
      .replace(/\r\n/g, "\n"),
  );
  const hash = crypto
    .createHash("sha1")
    .update(Buffer.concat([Buffer.from(`blob ${source.length}\0`), source]))
    .digest("hex");
  assert.equal(hash, sha, "React Bits upstream file changed: " + file);
}
const starts = [360, 390, 420, 450, 480, 510, 990, 1020, 1050];
let online = true,
  fail = false,
  scanMode = "running",
  rejectScan = false,
  hangScan = false;
const calls = [],
  assetRequests = new Set();
const state = {
  version,
  token: "test-session",
  rtc: true,
  storage: true,
  ratios: true,
  muted: false,
  led: {
    available: true,
    pin: rgbPin,
    mode: "hue",
    brightness: 128,
    period: 8000,
    red: 0,
    green: 184,
    blue: 169,
    stored: false,
    storageOK: true,
  },
  energy: 128432.728,
  power: 48.62,
  age: 1,
  ratio: 80,
  slave: 1,
  baud: 9600,
  parity: "N",
  crcErrors: 0,
  timeouts: 2,
  rejected: 0,
  heap: 180000,
  uptime: 11300,
  error: "Reading valid",
  error_code: "acquisition_ok",
  clock: "08:42:16",
  date: "2026-10-26",
  dayDate: "2026-10-26",
  slots: starts.map((start, i) => ({
    start,
    kwh: i < 5 ? [21.32, 24.85, 23.11, 25.68, 22.44][i] : i === 5 ? 9.12 : null,
    quality_code: i < 5 ? "complete_estimated" : i === 5 ? "active" : "waiting",
  })),
  scan: { phase: "idle", tested: 0, total: 0 },
};
const server = http.createServer((req, res) => {
  const asset = manifest.assets[req.url];
  if (asset) {
    assetRequests.add(req.url);
    res.writeHead(200, {
      "Content-Type": asset.mime,
      "Content-Encoding": "gzip",
      "Cache-Control": "no-store",
    });
    res.end(embedded[asset.symbol]);
    return;
  }
  res.setHeader("Content-Type", "application/json");
  if (req.url === "/api/status") {
    if (fail) {
      res.statusCode = 503;
      res.end("{}");
      return;
    }
    res.end(
      JSON.stringify({
        ...state,
        online: online && state.scan.phase !== "running",
      }),
    );
    return;
  }
  if (req.url === "/api/history") {
    res.end(
      JSON.stringify([
        { day: 20752, date: "2026-10-26" },
        { day: 20751, date: "2026-10-25" },
      ]),
    );
    return;
  }
  if (req.url.startsWith("/api/export")) {
    res.setHeader("Content-Type", "text/csv");
    res.setHeader(
      "Content-Disposition",
      "attachment; filename=fort-2026-10-26.csv",
    );
    res.end("date,shift,quality\n2026-10-26,morning,complete_estimated\n");
    return;
  }
  if (req.method === "POST") {
    let body = "";
    req.on("data", (chunk) => (body += chunk));
    req.on("end", () => {
      const data = new URLSearchParams(body);
      calls.push({ url: req.url, data });
      assert.equal(data.get("token"), state.token);
      if (req.url.startsWith("/api/scan/") && hangScan) return;
      if (req.url.startsWith("/api/scan/") && rejectScan) {
        res.statusCode = 503;
        res.end(
          JSON.stringify({
            code: "queue_busy",
            message: "Command queue is busy. Try again.",
          }),
        );
        return;
      }
      if (req.url === "/api/config")
        Object.assign(state, {
          slave: Number(data.get("slave")),
          baud: Number(data.get("baud")),
          parity: data.get("parity"),
        });
      if (req.url === "/api/scan/start")
        state.scan = {
          phase: scanMode,
          tested: scanMode === "running" ? 2 : 18,
          total: 18,
          first: Number(data.get("first")),
          last: Number(data.get("last")),
          elapsed: 3,
          slave: 7,
          baud: 19200,
          parity: "E",
          timeouts: 2,
          crcErrors: 1,
          rejected: 0,
          ...(scanMode === "found"
            ? { result: { slave: 7, baud: 19200, parity: "E" } }
            : {}),
        };
      if (req.url === "/api/scan/cancel") state.scan.phase = "cancelled";
      if (req.url === "/api/scan/apply")
        Object.assign(state, state.scan.result);
      if (req.url === "/api/led")
        Object.assign(state.led, {
          mode: data.get("mode"),
          brightness: Number(data.get("brightness")),
          period: Number(data.get("period")),
          red: Number(data.get("red")),
          green: Number(data.get("green")),
          blue: Number(data.get("blue")),
          stored: true,
          storageOK: true,
        });
      if (req.url === "/api/mute") state.muted = !state.muted;
      res.statusCode = 202;
      res.end(JSON.stringify({ code: "accepted", message: "Command queued." }));
    });
    return;
  }
  res.statusCode = 404;
  res.end("{}");
});

async function checkVectors(browser, out) {
  const assets = path.join(root, "docs/assets");
  const files = [];
  const collect = (dir) => {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      const file = path.join(dir, entry.name);
      if (entry.isDirectory()) collect(file);
      else if (file.endsWith(".svg")) files.push(file);
    }
  };
  collect(assets);
  const page = await browser.newPage();
  for (const file of files) {
    const svg = fs.readFileSync(file, "utf8"),
      box = svg.match(/viewBox="0 0 (\d+) (\d+)"/);
    assert(box, "SVG viewBox missing: " + file);
    await page.setViewportSize({
      width: Number(box[1]),
      height: Number(box[2]),
    });
    await page.setContent('<body style="margin:0">' + svg + "</body>");
    await page.evaluate(() => document.fonts.ready);
    const overflow = await page.evaluate(() => {
      const svg = document.querySelector("svg"),
        v = svg.viewBox.baseVal;
      return [...svg.querySelectorAll("text")]
        .filter((text) => {
          const b = text.getBBox();
          return (
            b.x < -0.5 ||
            b.y < -0.5 ||
            b.x + b.width > v.width + 0.5 ||
            b.y + b.height > v.height + 0.5
          );
        })
        .map((text) => text.textContent);
    });
    assert.deepEqual(overflow, [], "SVG text overflow: " + path.basename(file));
    if (!file.includes(path.sep + "badges" + path.sep))
      await page.screenshot({
        path: path.join(out, path.basename(file, ".svg") + ".png"),
        fullPage: true,
      });
  }
  await page.close();
  return files.length;
}

(async () => {
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const origin = `http://127.0.0.1:${server.address().port}`;
  const out = path.join(root, "output/previews"),
    assets = path.join(root, "docs/assets");
  fs.mkdirSync(out, { recursive: true });
  let browser;
  try {
    browser = await chromium.launch({
      headless: true,
      ...(process.argv[3] ? { channel: process.argv[3] } : {}),
    });
    if (process.argv.includes("--vectors-only")) {
      console.log(
        "PASS: " + (await checkVectors(browser, out)) + " SVGs rendered",
      );
      return;
    }
    const errors = [],
      external = [];
    const page = await browser.newPage({
      viewport: { width: 1440, height: 1050 },
    });
    page.on("pageerror", (error) => errors.push(error.message));
    await page.route("**/*", (route) => {
      if (!route.request().url().startsWith(origin)) {
        external.push(route.request().url());
        route.abort();
      } else route.continue();
    });
    const refresh = async () => {
      await page.locator("#refresh").click();
    };
    await page.goto(origin);
    await page.waitForFunction(
      () => document.querySelectorAll("#intervals tr").length === 6,
    );
    assert.equal(await page.locator("html").getAttribute("lang"), "en");
    assert.equal(await page.locator("#energy").textContent(), "128,432.728");
    assert.equal(
      await page.locator("#version").textContent(),
      "Firmware " + version,
    );
    await page.locator('[data-shift="1"]').click();
    assert.equal(await page.locator("#intervals tr").count(), 3);
    assert((await page.locator("#intervals").innerText()).includes("16:30"));
    assert((await page.locator(".shift-total").innerText()).includes("—")); // No complete samples, not zero energy.
    await page.locator('[data-shift="0"]').click();
    await page.locator("#nav-device").click();
    assert.equal(await page.locator("#led-mode").inputValue(), "hue");
    assert.equal(await page.locator("#led-brightness").inputValue(), "128");
    assert(await page.locator("#led-color").isDisabled());
    await page.locator("#led-mode").selectOption("blink");
    await page.locator("#led-color").fill("#1234ab");
    await page.locator("#led-period").fill("1.5");
    await page.locator("#led-brightness").fill("200");
    await refresh();
    await page.waitForTimeout(100);
    assert.equal(
      await page.locator("#led-mode").inputValue(),
      "blink",
      "Polling must preserve the unsaved LED draft",
    );
    await page.locator("#led-save").click();
    await page.waitForFunction(() =>
      document.getElementById("led-applied")?.textContent.includes("Blink"),
    );
    const ledCall = calls.find((call) => call.url === "/api/led");
    assert(ledCall);
    assert.equal(ledCall.data.get("red"), "18");
    assert.equal(ledCall.data.get("green"), "52");
    assert.equal(ledCall.data.get("blue"), "171");
    assert.equal(ledCall.data.get("brightness"), "200");
    assert.equal(ledCall.data.get("period"), "1500");
    await page.reload();
    await page.locator("#nav-device").click();
    await page.waitForFunction(
      () => document.getElementById("led-mode")?.value === "blink",
    );
    for (const mode of ["solid", "breathe", "off", "hue"]) {
      await page.locator("#led-mode").selectOption(mode);
      assert.equal(
        await page.locator("#led-period").isDisabled(),
        ["solid", "off"].includes(mode),
      );
      await page.locator("#led-save").click();
      await page.waitForFunction(
        (expected) =>
          document
            .getElementById("led-applied")
            ?.textContent.includes(expected),
        {
          solid: "Solid color",
          breathe: "Breathing",
          off: "Off",
          hue: "Hue rotation",
        }[mode],
      );
      await page.waitForFunction(
        () => !document.getElementById("led-save").disabled,
      );
    }
    state.led.available = false;
    await refresh();
    await page.waitForFunction(
      () => document.getElementById("led-save").disabled,
    );
    state.led.available = true;
    state.led.storageOK = false;
    await refresh();
    await page.waitForFunction(() =>
      document
        .querySelector(".led-panel .hint")
        ?.textContent.includes("Storage failed"),
    );
    state.led.storageOK = true;
    await refresh();
    await page.setViewportSize({ width: 390, height: 844 });
    await page.screenshot({
      path: path.join(root, "docs/assets/ui-led-mobile.png"),
      fullPage: true,
    });
    await page.setViewportSize({ width: 1440, height: 1000 });
    await page.screenshot({
      path: path.join(root, "docs/assets/ui-led-desktop.png"),
      fullPage: true,
    });
    await page.locator("#sync").click();
    await page.waitForFunction(() =>
      document.getElementById("message")?.textContent.includes("queued"),
    );
    assert(
      calls.some(
        (call) =>
          call.url === "/api/time" &&
          Number(call.data.get("epoch")) > 1704067200,
      ),
    );
    await page.locator("#history").selectOption("20751");
    const download = page.waitForEvent("download");
    await page.locator("#download").click();
    assert((await download).suggestedFilename().endsWith(".csv"));
    await page.locator("#nav-setup").click();
    await page.locator("#slave").fill("3");
    await page.locator("#baud").selectOption("4800");
    await page.locator("#parity").selectOption("O");
    await page.locator("#config button").click();
    await page.waitForFunction(
      () => !document.getElementById("scanstart").disabled,
    );
    assert(
      calls.some(
        (call) =>
          call.url === "/api/config" &&
          call.data.get("slave") === "3" &&
          call.data.get("baud") === "4800" &&
          call.data.get("parity") === "O",
      ),
    );
    const scanCalls = () =>
      calls.filter((call) => call.url.startsWith("/api/scan/"));
    await page.locator("#scanfirst").fill("20");
    await page.locator("#scanlast").fill("10");
    await page.locator("#scanstart").click();
    assert(
      (await page.locator("#message").innerText()).includes("Use slave IDs"),
    );
    assert.equal(scanCalls().length, 0);
    await page.locator("#scanfirst").fill("7");
    await page.locator("#scanlast").fill("7");
    await page.locator("#scanstart").click();
    await page.waitForFunction(
      () => !document.getElementById("scancancel").disabled,
    );
    assert.equal(scanCalls()[0].data.get("first"), "7");
    assert.equal(scanCalls()[0].data.get("last"), "7");
    assert(await page.locator("#slave").isDisabled());
    assert(await page.locator("#scanstart").isDisabled());
    assert(await page.locator("#scanapply").isDisabled());
    assert.equal(
      await page.locator("#scanprogress").getAttribute("value"),
      "2",
    );
    await page.locator("#language").selectOption("id");
    assert(
      (await page.locator("#scanstatus").innerText()).includes(
        "Pencatatan dijeda",
      ),
    );
    await page.reload();
    await page.waitForFunction(
      () =>
        document.getElementById("scancancel") &&
        !document.getElementById("scancancel").disabled,
    );
    assert.equal(await page.locator("html").getAttribute("lang"), "id");
    assert.equal(await page.locator("#scanfirst").inputValue(), "7");
    assert.equal(await page.locator("#scanlast").inputValue(), "7");
    await page.locator("#language").selectOption("en");
    await page.locator("#scancancel").click();
    await page.waitForFunction(() =>
      document.getElementById("scanstatus")?.textContent.includes("cancelled"),
    );
    assert.equal(await page.locator("#slave").inputValue(), "3");
    scanMode = "found";
    await page.locator("#scanstart").click();
    await page.waitForFunction(
      () => !document.getElementById("scanapply").disabled,
    );
    assert.equal(await page.locator("#slave").inputValue(), "3");
    assert((await page.locator("#scanstatus").innerText()).includes("ID 7"));
    await page.locator("#scanapply").click();
    await page.waitForFunction(
      () => document.getElementById("slave")?.value === "7",
    );
    assert.equal(await page.locator("#baud").inputValue(), "19200");
    assert.equal(await page.locator("#parity").inputValue(), "E");
    assert(await page.locator("#scanapply").isDisabled());
    scanMode = "exhausted";
    await page.locator("#scanstart").click();
    await page.waitForFunction(() =>
      document
        .getElementById("scanstatus")
        ?.textContent.includes("No meter found"),
    );
    rejectScan = true;
    await page.locator("#scanstart").click();
    await page.waitForFunction(() =>
      document.getElementById("message")?.textContent.includes("queue is busy"),
    );
    rejectScan = false;
    await page.locator("#language").selectOption("id");
    assert((await page.locator("#message").innerText()).includes("Antrean"));
    await page.locator("#language").selectOption("en");
    hangScan = true;
    await page.locator("#scanstart").click();
    await page.waitForFunction(
      () =>
        document
          .getElementById("message")
          ?.textContent.includes("No reply yet"),
      null,
      { timeout: 8000 },
    );
    hangScan = false;
    assert(!(await page.locator("#scanstart").isDisabled()));
    state.scan = {
      ...state.scan,
      phase: "running",
      first: 1,
      last: 247,
      tested: 54,
      total: 4446,
      elapsed: 46,
      slave: 55,
      baud: 9600,
      parity: "N",
    };
    await refresh();
    await page.waitForFunction(
      () =>
        document.getElementById("scanprogress")?.getAttribute("value") === "54",
    );
    if (await page.locator("#dismiss").count())
      await page.locator("#dismiss").click();
    await page.waitForTimeout(400);
    await page.screenshot({
      path: path.join(assets, "ui-scan-desktop.png"),
      fullPage: true,
    });
    await page.setViewportSize({ width: 390, height: 844 });
    await page.screenshot({
      path: path.join(assets, "ui-scan-mobile.png"),
      fullPage: true,
    });
    for (const width of [320, 390, 768]) {
      await page.setViewportSize({ width, height: 844 });
      assert(
        await page.evaluate(
          () => document.documentElement.scrollWidth <= innerWidth,
        ),
        "setup overflow at " + width,
      );
    }
    state.scan.phase = "cancelled";
    await refresh();
    await page.waitForFunction(() =>
      document.getElementById("scanstatus")?.textContent.includes("cancelled"),
    );
    await page.locator("#nav-overview").click();
    await page.waitForTimeout(400);
    await page.setViewportSize({ width: 1440, height: 1050 });
    await page.screenshot({
      path: path.join(assets, "ui-desktop.png"),
      fullPage: true,
    });
    await page.setViewportSize({ width: 390, height: 844 });
    await page.screenshot({
      path: path.join(assets, "ui-mobile.png"),
      fullPage: true,
    });
    await page.locator("#language").selectOption("id");
    assert.equal(await page.locator("#energy").textContent(), "128.432,728");
    assert((await page.locator("#intervals").innerText()).includes("Lengkap"));
    await page.screenshot({
      path: path.join(assets, "ui-mobile-id.png"),
      fullPage: true,
    });
    await page.setViewportSize({ width: 1440, height: 1050 });
    await page.screenshot({
      path: path.join(assets, "ui-desktop-id.png"),
      fullPage: true,
    });
    for (const width of [320, 390, 768, 1440, 2560]) {
      await page.setViewportSize({ width, height: 1000 });
      assert(
        await page.evaluate(
          () => document.documentElement.scrollWidth <= innerWidth,
        ),
        "overview overflow at " + width,
      );
    }
    online = false;
    await refresh();
    await page.waitForFunction(() =>
      document.getElementById("connection")?.textContent.includes("offline"),
    );
    assert.equal(await page.locator("#power").textContent(), "—");
    fail = true;
    await refresh();
    await page.waitForFunction(() =>
      document.getElementById("connection")?.textContent.includes("terputus"),
    );
    assert(
      (await page.locator("#health").innerText()).includes(
        "bukan pembacaan langsung",
      ),
    );
    await page.locator("#nav-setup").click();
    assert(await page.locator("#scanstart").isDisabled());
    assert(await page.locator("#scanapply").isDisabled());
    await page.locator("#nav-device").click();
    assert(await page.locator("#led-save").isDisabled());
    assert((await page.locator("#led-title").innerText()).includes("bawaan"));
    fail = false;
    online = true;
    const reduced = await browser.newPage({
      viewport: { width: 390, height: 844 },
      reducedMotion: "reduce",
    });
    reduced.on("pageerror", (error) => errors.push(error.message));
    await reduced.addInitScript(() => {
      Object.defineProperty(window, "localStorage", {
        get() {
          throw new Error("Storage blocked");
        },
      });
    });
    await reduced.goto(origin);
    await reduced.waitForSelector("#energy");
    assert.equal(await reduced.locator("html").getAttribute("lang"), "en");
    assert.equal(
      await reduced
        .locator(".energy-surface")
        .evaluate((node) => getComputedStyle(node).visibility),
      "visible",
    );
    await reduced.close();
    assert.deepEqual(external, []);
    assert.deepEqual(errors, []);
    assert(
      assetRequests.has("/assets/app.js") &&
        assetRequests.has("/assets/app.css"),
    );
    await page.close();
    const vectors = await checkVectors(browser, out);
    console.log(
      `PASS: real gzip assets + upstream React Bits hashes; EN/ID persistence and errors; LED modes/POST/draft/reload/storage/disabled; scan/config/time/CSV; stale/offline; 320–2560px; reduced motion; no external requests/JS errors; ${vectors} SVGs`,
    );
  } catch (error) {
    const failed = browser?.contexts()[0]?.pages()[0];
    if (failed) {
      await failed.screenshot({
        path: path.join(out, "ui-failure.png"),
        fullPage: true,
      });
      fs.writeFileSync(
        path.join(out, "ui-failure.txt"),
        await failed.locator("body").innerText(),
      );
    }
    throw error;
  } finally {
    if (browser) await browser.close();
    await new Promise((resolve) => server.close(resolve));
  }
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
