FROM python:3.8.13 
        
WORKDIR /app

RUN apt-get update 
RUN pip install --upgrade pip

# #rustc
# curl https://sh.rustup.rs -sSf | sh &&  source "$HOME/.cargo/env" &&\
#npm
RUN  apt-get install npm -y 
#nvm
SHELL ["/bin/bash", "--login", "-c"]
RUN curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.35.3/install.sh | bash
RUN nvm install 14.7
#poetry
RUN   pip install poetry==1.0.0 
#pyenv
RUN   apt-get install python3-venv -y   
RUN   git clone https://github.com/nccasia/komutracker.git 
WORKDIR /app/komutracker
RUN     git clone https://github.com/nccasia/aw-client.git &&\
        git clone https://github.com/nccasia/aw-qt.git &&\
        git clone https://github.com/nccasia/aw-watcher-afk.git &&\
        git clone https://github.com/nccasia/aw-watcher-window.git 
COPY    aw-server aw-server
COPY    aw-core aw-core 

RUN     python3 -m venv venv &&\
        source /app/komutracker/venv/bin/activate &&\
        make build

WORKDIR /app/komutracker/aw-server
RUN     source /app/komutracker/venv/bin/activate && poetry install

WORKDIR /app/komutracker
RUN     source /app/komutracker/venv/bin/activate &&\
        pip install pymongo &&\
        pip install waitress
EXPOSE 5600
ENTRYPOINT source /app/komutracker/venv/bin/activate && aw-server
