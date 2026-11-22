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

    DepGraph_AddArtifactDep(graph, idx, idx2);

    NodeIdx idx3 = DepGraph_CreateInstalledNode(graph, (KPM_InstalledPackage) {
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
    ASSERT_HD(idx3, 2);

    DepGraph_AddDepTarget(graph, idx2, idx3);

    char *rendered;
    DepGraph_Render(graph, &rendered);
    kpmio.log(KPM_VERBOSITY_DEBUG, "%s", rendered);
    free(rendered);

    DepGraph_FreeGraph(graph);
    free(cur_path);
    return 0;
}