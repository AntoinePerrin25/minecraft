#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    // Création des dossiers nécessaires
    nob_mkdir_if_not_exists("build");
    nob_mkdir_if_not_exists("src");

    // Compilation du client
    {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "gcc");
        nob_cmd_append(&cmd, "-Wall", "-Wextra", "-O2");
        nob_cmd_append(&cmd, "-I./include");
        nob_cmd_append(&cmd, "-pthread");  // Add pthread support
        nob_cmd_append(&cmd, "./src/main.c", "./src/rnet.c");  // Include rnet.c in compilation
        nob_cmd_append(&cmd, "-o", "./build/game");
        nob_cmd_append(&cmd, "-L./lib");
        nob_cmd_append(&cmd, "-lraylib", "-lenet", "-lopengl32", "-lgdi32", "-lwinmm", "-lws2_32");
        if (!nob_cmd_run_sync(cmd)) return 1;
    }

    // Compilation du serveur
    {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "gcc");
        nob_cmd_append(&cmd, "-Wall", "-Wextra", "-O2");
        nob_cmd_append(&cmd, "-pthread");  // Add pthread support
        nob_cmd_append(&cmd, "-I./include");
        nob_cmd_append(&cmd, "./src/server.c", "./src/rnet.c");  // Include rnet.c in compilation
        nob_cmd_append(&cmd, "-o", "./build/server");
        nob_cmd_append(&cmd, "-L./lib");
        nob_cmd_append(&cmd, "-lenet", "-lws2_32");
        if (!nob_cmd_run_sync(cmd)) return 1;
    }

    return 0;
}