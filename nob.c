#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    // Création des dossiers nécessaires
    nob_mkdir_if_not_exists("src");

    // Compilation du client
    {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "gcc");
        nob_cmd_append(&cmd, "-Wall", "-Wextra", "-O3", "-ffast-math", "-march=native", "-lpthread");
        nob_cmd_append(&cmd, "-I./include");
        nob_cmd_append(&cmd, "./src/main.c");
        nob_cmd_append(&cmd, "./src/data.c");
        nob_cmd_append(&cmd, "./src/atlas.c");
        nob_cmd_append(&cmd, "-o", "./game");
        nob_cmd_append(&cmd, "-L./lib");
        nob_cmd_append(&cmd, "-lraylib", "-lopengl32", "-lgdi32", "-lwinmm", "-lws2_32");
        if (!nob_cmd_run_sync(cmd)) 
        {
            printf("Client Not Compiled\n");
            return 1;
        }
        else printf("Client Compiled Successully\n");
    }


    // Lancement du client
    if(!strcmp(argv[1], "l"))
    {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, ".\\game.exe");
        nob_cmd_run_sync(cmd);
    }

    return 0;
}