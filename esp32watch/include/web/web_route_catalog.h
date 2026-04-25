#ifndef WEB_ROUTE_CATALOG_H
#define WEB_ROUTE_CATALOG_H

#include <stddef.h>

struct WebRouteOperationCatalogEntry {
    const char *method;
    const char *schema_kind;
    const char *schema_name;
    const char *const *request_fields;
    size_t request_field_count;
    const char *const *response_fields;
    size_t response_field_count;
};

struct WebRouteCatalogEntry {
    const char *route_key;
    const char *route_path;
    const char *surface_tier;
    const char *producer_owner;
    const char *consumer_owner;
    const char *deprecation_policy;
    const char *default_method;
    const WebRouteOperationCatalogEntry *operations;
    size_t operation_count;
};

struct WebApiSchemaCatalogEntry {
    const char *name;
    const char *const *required;
    size_t required_count;
    const char *const *sections;
    size_t section_count;
};

struct WebStateSectionCatalogEntry {
    const char *section;
    const char *const *flags;
    size_t flag_count;
};

struct WebStateSchemaCatalogEntry {
    const char *name;
    const WebStateSectionCatalogEntry *sections;
    size_t section_count;
};

#endif
