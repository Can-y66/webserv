# WebServ (C)

Petit serveur web d'apprentissage écrit en C. Il gère des routes dynamiques, parse les paramètres GET, log les requêtes, et sert les fichiers statiques depuis `www/`.

## Build

```bash
make
```

## Lancer

```bash
./webserver 8080
```

Puis ouvre :

- http://localhost:8080/
- http://localhost:8080/hello?name=Codex
- http://localhost:8080/test
- http://localhost:8080/api?foo=bar

## Notes

- Les fichiers statiques sont servis depuis `www/`.
- Les requêtes POST renvoient une page de confirmation avec le body reçu.
- Les logs sortent sur stdout avec l'heure et l'IP client.
