// Copy this file to secrets.h (gitignored) and fill in your values.
#pragma once

#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// Realtime Database URL, with or without a trailing slash, e.g.
//   https://<project-id>-default-rtdb.firebaseio.com
//   https://<project-id>-default-rtdb.europe-west1.firebasedatabase.app
#define FIREBASE_HOST "https://your-project-id-default-rtdb.firebaseio.com"

// Optional. Leave empty when /images is publicly readable (recommended rules in
// the README). Otherwise a database secret or an ID token, sent as ?auth=.
#define FIREBASE_AUTH ""
