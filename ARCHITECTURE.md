# Architecture du projet

## Architecture générale

### Représentation sous forme d'arbre des fichiers important du code source

```
/
├─┬─include/sy5/: Les fichiers d'en-têtes du projet.
│ └─┬─array.h: Fonctions permettant de représenter un tableau dynamique.
│   ├─common.h: Variables partagés entre cassini et saturnd.
│   ├─reply.h: Structure permettant de représenter une réponse.
│   ├─request.h: Structure permettant de représenter une requête.
│   ├─types.h: Structures principales nécessaire au projet.
│   ├─utils.h: Fonctions utilitaires.
│   └─worker.h: Fonctions faisant tourner une tâche (dans un thread).
├─src/: Implémentation des en-têtes.
└─**/**.*: Autres fichiers.
```

### Répartition du code source

Une grande partie du code source est partagée entre `cassini` et `saturnd` (structures, lecture des données, écriture des données, etc.), pour éviter la copie de code à tout va, le code partagé est donc inclus dans des fichiers `.c` non spécifique à `cassini` ou `saturnd` qui sont compilé pour les deux programmes. Uniquement le fichier `cassini.c` est spécifique à `cassini` et les fichiers `saturnd.c` et `worker.c` sont spécifique à `saturnd`.

### Autres points intéressant

- Chaque type évoqué dans le fichier `protocole.md` est défini par sa propre structure de données dans le fichier `types.h` et possède son lot de fonctions utilitaires dans `utils.h`.
- La compilation de `cassini` et `saturnd` sont séparé en deux tâches différentes dans le `Makefile` et `CMakeLists.txt`.

## Architecture de `cassini`

La fonction `main` de `cassini` peut être trouvée dans `cassini.c`.

L'architecture de ce programme est relativement simple, il évalue d'abord les options de l'utilisateur puis il regarde s'il est capable de trouver la pipe de requête du démon aux chemins d'accès voulu. Il réalise (au maximum) 10 tentatives d'ouverture de ce fichier, s'il n'y arrive pas, le programme termine avec une erreur (car le démon ne semble pas accessible). Sinon l'exécution continue en envoyant la requête voulue sous forme d'un `buffer` qui contient tous les données de la requête puis il attend la réponse en ouvrant la pipe de réponse du démon et lit élément par élément son contenu (qui diffère selon la réponse envoyée). Finalement, il traite cette réponse et termine avec succès.

## Architecture de `saturnd`

La fonction `main` de `saturnd` peut être trouvée dans `saturnd.c`.

Comme `cassini`, il évalue les options. Après cela, il vérifie si un démon n'est pas déjà accessible au chemin d'accès voulu (en tentant d'y envoyer une requête comme le ferait `cassini`), si c'est le cas il termine avec une erreur. Sinon, il crée si nécessaire les dossiers `pipes` et `tasks` ainsi que les pipes de requête et de réponse. Puis il regarde s'il existe des tâches déjà existante (d'une ancienne exécution du démon) dans le dossier `tasks`, si c'est le cas, il les lit pour pouvoir les réaliser. Il rentre ensuite dans une boucle qui continuera de s'exécuter tant que le démon ne reçoit pas de demande d'extinction. Cette boucle commence par ouvrir la pipe de requête pour attendre d'en recevoir une, il va ensuite la lire élément par élément (qui diffère selon la requête envoyée) et la traitera avant d'envoyer la réponse voulue. Lorsque la boucle est quittée (une demande d'extinction a été reçue et traitée), il termine avec succès.

Chaque tâche est exécutée dans un `pthread` qui lui est unique, lorsque la demande de création d'une tâche est reçu, ses informations seront sauvegardées dans des fichiers (`task`, `runs`, `last_stdout`, `last_stderr`) dans un dossier nommé par son `taskid` et son exécution sera géré par son thread. Le thread vérifie d'abord si la tâche a déjà été exécuté cette minute (possible dans le cas ou le démon a été arrêté et relancé la même minute), si c'est le cas il attend jusqu'à la prochaine minute. Ensuite, une boucle est lancée tant que la tâche n'est pas supprimée. Cette boucle vérifie d'abord si la tâche doit être exécutée cette minute, si la réponse est non, le thread attend la prochaine minute. Sinon il l'exécute dans un `fork` à l'aide d'un `execvp` et récupère tous les données voulus (`time`, `exitcode`, `stdout`, `stderr`) et stocke les résultats dans les fichiers respectifs avant d'attendre jusqu'à la prochaine minute pour répéter la boucle.