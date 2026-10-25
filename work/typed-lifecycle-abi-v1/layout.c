#include "core/extension/gdextension_interface.gen.h"
#include <stddef.h>
#include <stdio.h>
_Static_assert(sizeof(GDExtensionDeviceExitGuardToken) == 16, "guard handle");
_Static_assert(sizeof(((GDExtensionDeviceExitGuardRegistrationV1 *)0)->needs) == 32, "four inline level needs");
_Static_assert(sizeof(((GDExtensionDeviceExitGuardRegistrationV1 *)0)->expected_initialization) == sizeof(GDExtensionInitialization), "by value tuple");
_Static_assert(sizeof(GDExtensionDeviceExitExitDiagnosticV1) == 32, "diagnostic");
int main(void) {
 printf("abi=%zu registration=%zu diagnostic=%zu needs=%zu initialization=%zu\n", sizeof(GDExtensionDeviceExitAbi), sizeof(GDExtensionDeviceExitGuardRegistrationV1), sizeof(GDExtensionDeviceExitExitDiagnosticV1), offsetof(GDExtensionDeviceExitGuardRegistrationV1, needs), offsetof(GDExtensionDeviceExitGuardRegistrationV1, expected_initialization));
}
