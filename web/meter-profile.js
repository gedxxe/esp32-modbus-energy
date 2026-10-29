// The production catalog comes from the firmware's shared MeterProfiles table.
// This Fort fallback lets an older device remain readable during an upgrade.
export const legacyFort={key:"fort",name:"Fort",primaryFloat:138,terminals:"58 / A · 59 / B",maxBaud:38400,energyScale:.001,ratios:44,ratioWords:3,currentA:{address:88,words:2},voltageA:{address:66,words:2},power:{address:106,words:2},frequency:{address:128,words:1},secondaryEnergy:{address:130,words:2}};
export const meterProfile=data=>data?.meter||legacyFort;
export const meterProfiles=data=>data?.meterProfiles||[legacyFort];
export const addressHex=value=>Number(value).toString(16).toUpperCase().padStart(4,"0");
export const profileName=(key,catalog=[legacyFort])=>catalog.find(p=>p.key===(key||"fort"))?.name||key;

export const probeRegister=profile=>profile.probe||profile.currentA;
export const directEnergy=profile=>profile.energyBasis==="meter-total";
export const energyRegister=profile=>profile.meterEnergy||profile.secondaryEnergy;
