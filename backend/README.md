# Commander Chess Backend (Cloud Run)

## Local Docker test

```bash
docker build -t commanderchess-api ./backend
docker run --rm -p 8080:8080 commanderchess-api
```

In another terminal:

```bash
curl http://127.0.0.1:8080/health
```

## Cloud Run deploy

```bash
gcloud run deploy commanderchess-api --source backend --region us-central1 --allow-unauthenticated
```
