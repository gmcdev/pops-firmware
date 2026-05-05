#pragma once

// POST to /api/devices/auth with an HMAC-SHA256 signed payload.
// Stores the returned session token in memory on success.
// Returns true on successful authentication.
bool authAuthenticate();

// Returns true if a session token is held in memory.
bool authIsAuthenticated();

// Returns the stored session token string, or nullptr if not authenticated.
const char* authGetSessionToken();
