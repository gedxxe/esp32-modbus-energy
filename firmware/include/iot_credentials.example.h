// Copy to firmware/include/iot_credentials.local.h (ignored by Git).
// DEVELOPMENT DEVICES ONLY. Values must match an explicitly registered
// meter/source in the website's development database; production remains off.
#pragma once
#define IOT_DEV_ENABLED 1
#define IOT_DEV_ENDPOINT "https://YOUR-DEVELOPMENT-DEPLOYMENT.vercel.app/api/iot-ingest"
#define IOT_DEV_DEVICE_ID "dev_source"
#define IOT_DEV_METER_ID "meter_1"
#define IOT_DEV_METER_PROFILE "fort"
#define IOT_DEV_BEARER "PASTE_43_CHARACTER_BASE64URL_KEY_FROM_PRIVATE_DEVICE_REGISTRATION"
