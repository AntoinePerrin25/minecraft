#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    // Création des dossiers nécessaires
    nob_mkdir_if_not_exists("build");
    nob_mkdir_if_not_exists("src");
    nob_mkdir_if_not_exists("tests");

    
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "gcc");
    nob_cmd_append(&cmd, "-Wall", "-Wextra", "-O3");
    nob_cmd_append(&cmd, "-I./include");
    nob_cmd_append(&cmd, "./src/chunk_manager.c", "./src/tests.c", "");
    nob_cmd_append(&cmd, "-o", "./tests/tests");
    nob_cmd_append(&cmd, "-L./lib");
    nob_cmd_append(&cmd, "-lraylib", "-lenet", "-lopengl32", "-lgdi32", "-lwinmm", "-lws2_32");
    if (!nob_cmd_run_sync(cmd)) return 1;
    
    /*
    cmd = (Nob_Cmd){0};
    nob_cmd_append(&cmd, "./tests/tests");
    nob_cmd_run_async(cmd);
    if (!nob_cmd_run_sync(cmd)) return 1;
    
    printf("Tests Executed\n");
    */

    return 0;
}