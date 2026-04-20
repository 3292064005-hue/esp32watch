#include "ui_page_module_manifest.h"

void ui_page_module_manifest_register_providers(void)
{
    (void)ui_page_module_register_provider("core", ui_page_register_core_module);
}
