#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    // Création des dossiers nécessaires
    nob_mkdir_if_not_exists("build");
    nob_mkdir_if_not_exists("src");
    nob_mkdir_if_not_exists("tests");

    // Compilation du serveur
    {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "gcc");
        nob_cmd_append(&cmd, "-Wall", "-Wextra", "-Werror", "-O3", "-g");
        nob_cmd_append(&cmd, "-I./include", "-L./lib");
        nob_cmd_append(&cmd, "-I./src");
        nob_cmd_append(&cmd, "./src/server.c", "./src/world_manager.c", "./src/chunk_manager.c");
        nob_cmd_append(&cmd, "-o", "./build/server");
        nob_cmd_append(&cmd, "-lraylib", "-lenet", "-lopengl32", "-lgdi32", "-lwinmm", "-lws2_32");
        if (!nob_cmd_run_sync(cmd)) 
        {
            printf("Server Not Compiled\n");
            return 1;
        }
        else printf("Server Compiled Successully\n");

    }
    
    // Compilation du client
    {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "gcc");
        nob_cmd_append(&cmd, "-Wall", "-Wextra", "-Werror", "-O3", "-g");
        nob_cmd_append(&cmd, "-I./include/", "-L./lib/");
        nob_cmd_append(&cmd, "./src/chunk_manager.c", "./src/main.c", "./src/world_manager.c");
        nob_cmd_append(&cmd, "-o", "./build/game");
        nob_cmd_append(&cmd, "-lraylib", "-lenet", "-lopengl32", "-lgdi32", "-lwinmm", "-lws2_32");
        if (!nob_cmd_run_sync(cmd)) 
        {
            printf("Client Not Compiled\n");
            return 1;
        }
        else printf("Client Compiled Successfully\n");
    }


    if(strcmp(argv[1], "l") || (strcmp(argv[1], "launch")))

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