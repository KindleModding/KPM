#include "internal_utils.h"
#include "kpm_test_common.h"
#include "kpm_test_io.h"
#include "dependencies.h"
#include "kpkg.h"
#include "kpm.h"
#include "kpmio.h"
#include "semver.h"
int main(int argc, char *argv[])
{
    char *cur_path = get_path(argc, argv);
    DependencyGraph *graph = DepGraph_Create(0);
    DepGraph_FreeGraph(graph);

    graph = DepGraph_Create(10);
    DepGraph_FreeGraph(graph);

    graph = DepGraph_Create(0);

    KPM_InstallTarget target = {
        .type = INSTALL_FILE
    };

    char *path = asprintf_hd("%s/resources/test_kpkg/kpkg3_subcat_icon_1.0.0.kpkg", cur_path);
    ASSERT_HD_KPKG(KPKG_LoadFile(&target.file, path, "./tmp.bin", &kpmio), KPKG_OK);
    free(path);
    NodeIdx idx = DepGraph_CreateTargetNode(graph, target, GT_PRIORITY_EXPLICIT);
    ASSERT_HD(idx, 0);

    NodeIdx idx2 = DepGraph_CreateDependencyNode(graph, (KPM_Dependency) {
        .id = strdup("dependency_id"),
        .min_version = SEMVER_MIN,
        .max_version = SEMVER_MAX
    });
    ASSERT_HD(idx2, 1);

    DepGraph_AddTargetDep(graph, idx, idx2);

    NodeIdx idx3 = DepGraph_CreateInstalledNode(graph, (KPM_InstalledPackage) {
        .id = strdup("installed_id"),
        .repository = NULL,
        .name = strdup("Installed Name"),
        .author = strdup("Installed Author"),
        .description = strdup("Installed Description"),
        .category = strdup("book"),
        .subcategory = NULL,
        .installed_size = 32,
        .version = SEMVER_MIN,
        .installed_as_dependency = false,
    });
    ASSERT_HD(idx3, 2);
    DepGraph_AddDepTarget(graph, idx2, idx3);

    NodeIdx idx4 = DepGraph_CreateInstalledNode(graph, (KPM_InstalledPackage) {
        .id = strdup("installed_id"),
        .repository = NULL,
        .name = strdup("Installed Name"),
        .author = strdup("Installed Author"),
        .description = strdup("Installed Description"),
        .category = strdup("book"),
        .subcategory = NULL,
        .installed_size = 32,
        .version = { 1, 2, 3},
        .installed_as_dependency = false,
    });
    ASSERT_HD(idx4, 3);
    DepGraph_AddDepTarget(graph, idx2, idx4);

    NodeIdx idx5 = DepGraph_CreateInstalledNode(graph, (KPM_InstalledPackage) {
        .id = strdup("installed_id"),
        .repository = NULL,
        .name = strdup("Installed Name"),
        .author = strdup("Installed Author"),
        .description = strdup("Installed Description"),
        .category = strdup("book"),
        .subcategory = NULL,
        .installed_size = 32,
        .version = SEMVER_MAX,
        .installed_as_dependency = false,
    });
    ASSERT_HD(idx5, 4);
    DepGraph_AddDepTarget(graph, idx2, idx5);

    SemVer version = {1,0,0};
    ASSERT_HD_SEMVER(DepGraph_GetTargetVersion(graph->nodes[idx].gt), version);
    ASSERT_HD_SEMVER(DepGraph_GetTargetVersion(graph->nodes[idx2].gt), SEMVER_MIN);
    ASSERT_HD_SEMVER(DepGraph_GetTargetVersion(graph->nodes[idx3].gt), SEMVER_MIN);
    version = (SemVer){1,2,3};
    ASSERT_HD_SEMVER(DepGraph_GetTargetVersion(graph->nodes[idx4].gt), version);
    ASSERT_HD_SEMVER(DepGraph_GetTargetVersion(graph->nodes[idx5].gt), SEMVER_MAX);

    ASSERT_HD(DepGraph_FindTargetInGraph(graph, graph->nodes[idx].gt), idx);
    ASSERT_HD(DepGraph_FindDependency(graph, graph->nodes[idx2].gt.dependency), idx2);
    ASSERT_HD(DepGraph_FindTargetInGraph(graph, graph->nodes[idx3].gt), idx3);
    ASSERT_HD(DepGraph_FindTargetInGraph(graph, graph->nodes[idx4].gt), idx4);
    ASSERT_HD(DepGraph_FindTargetInGraph(graph, graph->nodes[idx5].gt), idx5);
    ASSERT_HD(graph->nodes[idx2].connected->count, 3);
    ASSERT_HD(graph->nodes[idx2].connected->items[0], idx5);
    ASSERT_HD(graph->nodes[idx2].connected->items[1], idx4);
    ASSERT_HD(graph->nodes[idx2].connected->items[2], idx3);

    NodeIdx idx6 = DepGraph_CreateDependencyNode(graph, (KPM_Dependency) {
        .id = strdup("installed_id"),
        .min_version = SEMVER_MIN,
        .max_version = SEMVER_MAX
    });

    DepGraph_AddTargetDep(graph, idx, idx6);
    DepGraph_AddExistingTargetsToDependency(graph, idx6);
    ASSERT_HD(graph->nodes[idx6].connected->count, 2);
    ASSERT_HD(graph->nodes[idx6].connected->items[0], idx4);
    ASSERT_HD(graph->nodes[idx6].connected->items[1], idx3);

    NodeIdx idx7 = DepGraph_CreateInstalledNode(graph, (KPM_InstalledPackage) {
        .id = strdup("installed_id"),
        .repository = NULL,
        .name = strdup("Installed Name"),
        .author = strdup("Installed Author"),
        .description = strdup("Installed Description"),
        .category = strdup("book"),
        .subcategory = NULL,
        .installed_size = 32,
        .version = (SemVer){1, 1, 3},
        .installed_as_dependency = false,
    });
    DepGraph_AddTargetToExistingDependencies(graph, idx7);
    ASSERT_HD(graph->nodes[idx6].connected->count, 3);
    ASSERT_HD(graph->nodes[idx6].connected->items[0], idx4);
    ASSERT_HD(graph->nodes[idx6].connected->items[1], idx7);
    ASSERT_HD(graph->nodes[idx6].connected->items[2], idx3);

    char *rendered;
    DepGraph_Render(graph, &rendered);
    kpmio.log(KPM_VERBOSITY_DEBUG, "%s", rendered);
    free(rendered);

    DepGraph_FreeGraph(graph);
    free(cur_path);
    return 0;
}