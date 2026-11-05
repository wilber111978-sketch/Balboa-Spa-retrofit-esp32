// Static local server defaults to the safe simulator. The integrated ESP32 route
// replaces this module with {mode:'live',apiVersion:1} on the controller origin.
export const runtime = Object.freeze({mode:'demo',apiVersion:1});
