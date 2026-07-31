#pragma once

#ifdef BATTERY_MONITOR_CI
#include "ci_secrets.h"
#else
#include "secrets.h"
#endif

// TLS is optional. Existing private secrets.h files from before TLS support
// keep using HTTP until the operator opts in explicitly.
#ifndef SERVER_USE_TLS
#define SERVER_USE_TLS 0
#endif

#ifndef SERVER_ROOT_CA
#define SERVER_ROOT_CA ""
#endif

#if SERVER_USE_TLS
static_assert(sizeof(SERVER_ROOT_CA) > 256,
              "secrets.h: SERVER_ROOT_CA must contain the PEM root CA used "
              "to validate the backend certificate.");
#endif
