# build

cd ~/komutracker 

docker-compose -f mongo.yaml build

docker-compose -f mongo.yaml up

# data
  + Local (Linux/Mac): docker volume location: /var/lib/docker/volumes
  + Container: /data/db

# server
http://localhost:5600
