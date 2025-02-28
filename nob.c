#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    // Création des dossiers nécessaires
    nob_mkdir_if_not_exists("build");
    nob_mkdir_if_not_exists("src");
    nob_mkdir_if_not_exists("tests");

    // tests ?

    if(strcmp(argv[1], "tests") == 0)
    {
        // Compilation des tests
        {
            Nob_Cmd cmd = {0};
            nob_cmd_append(&cmd, "gcc");
            nob_cmd_append(&cmd, "-Wall", "-Wextra", "-O3");
            nob_cmd_append(&cmd, "-I./include");
            nob_cmd_append(&cmd, "./tests/tests.c");
            nob_cmd_append(&cmd, "-o", "./tests/tests");
            nob_cmd_append(&cmd, "-L./lib");
            nob_cmd_append(&cmd, "-lraylib", "-lenet", "-lopengl32", "-lgdi32", "-lwinmm", "-lws2_32");
            if (!nob_cmd_run_sync(cmd)) return 1;
        }

        // Exécution des tests
        {
            Nob_Cmd cmd = {0};
            nob_cmd_append(&cmd, "./tests/tests");
            nob_cmd_run_async(cmd);
        }

        return 0;
    }



    // Compilation du client
    {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "gcc");
        nob_cmd_append(&cmd, "-Wall", "-Wextra", "-O2");
        nob_cmd_append(&cmd, "-I./include");
        nob_cmd_append(&cmd, "./src/main.c");
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
        nob_cmd_append(&cmd, "-I./include");
        nob_cmd_append(&cmd, "./src/server.c");
        nob_cmd_append(&cmd, "-o", "./build/server");
        nob_cmd_append(&cmd, "-L./lib");
        nob_cmd_append(&cmd, "-lenet", "-lws2_32");
        if (!nob_cmd_run_sync(cmd)) return 1;
    }

    // Lancement du serveur
    {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "./build/server");
        nob_cmd_run_async(cmd);
    }

    // Lancement du client
    {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "./build/game");
        nob_cmd_run_async(cmd);
    }

    return 0;
}