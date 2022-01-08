# Saturnd

Saturnd is a daemon capable of scheduling tasks on Unix operating systems to run any command at a given time, with the ability to get the informations of every run. Cassini, a command-line interface can be used with various options to communicate with the daemon from a client-side.

## Features

- Schedule tasks to run any commandline and specify when it should run.
- Get at the last stdout and stderr of any scheduled task.
- Get informations about every run of any scheduled task (execution time and exit code).
- Automatic saving of scheduled tasks (they will be resume at daemon startup).

## How to use

The project can be built by using either make or cmake based on your workflow.
To build it using make, simply run the `make` command, the `saturnd` and `cassini` executables should be created.
Once the project is built, you can run the daemon with `./saturnd` and you can
send commands to it using `./cassini`, to know what you can do precisely,
run the `./cassini -h`.

## Contributors

- [KINDEL Hugo](https://gaufre.informatique.univ-paris-diderot.fr/hugokindel)
- [JAUROYON Maxime](https://gaufre.informatique.univ-paris-diderot.fr/jauroyon)

## License

This project is made for educational purposes only and any part of it can be used freely.