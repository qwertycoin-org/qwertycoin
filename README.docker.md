# Build and run with docker

## Build with 'docker build'

```bash
git clone https://github.com/qwertycoin-org/qwertycoin.git
cd qwertycoin
docker build -t local/qwertycoin:latest
```

### with buildx (multiarch)
currently only compatible with amd 
```bash
docker buildx build \
	-t local/qwertycoin \
	--platform=linux/amd64,linux/arm64,linux/arm/v7,darwin \
	.
```

## Run

```bash
docker run -d \
	-p 8196:8196 \
	-p 8197:8197 \
	-e CONTACT=dev@qwertycoin.org \
	-e FEE_ADDRESS=QWC1Dae7oKkhk2mxueQvtDJJWeowVB1i1fhfSnL4n3GnG3KgdbE9tS4iPgVns2Gj7JgrSjQt6W3oP4ZeRyE1sdAp7KxUMQCDoT \
	-v /blockchain/path:/blockchain \
	--name qwertycoin \
	qwertycoin/qwertycoin:latest
```

## Update

```
docker stop qwertycoin
docker rm qwertycoin
docker pull qwertycoin/qwertycoin
docker run ... # above mentioned
```

### update easy way

using ["watchtower"](https://github.com/containrrr/watchtower) to update and rerun container

```bash
docker run -d --rm \
    -v /var/run/docker.sock:/var/run/docker.sock \
    containrrr/watchtower \
    qwertycoin
```

## run with docker-compose

docker-compose.yml
```yaml
version: "3"
services:
  qwertycoin:
    image: qwertycoin/qwertycoin:latest
    container_name: qwertycoin
    environment:
      - CONTACT=dev@qwertycoin.org 
      - FEE_ADDRESS=QWC1Dae7oKkhk2mxueQvtDJJWeowVB1i1fhfSnL4n3GnG3KgdbE9tS4iPgVns2Gj7JgrSjQt6W3oP4ZeRyE1sdAp7KxUMQCDoT
    ports:
      - "8196:8196"
      - "8197:8197"
    volumes:
      - /home/{USER}/.Qwertycoin:/blockchain
      - /home/{USER}/.Qwertycoin/logs:/logs
```

## List of environment variables

todo

