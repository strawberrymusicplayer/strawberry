FROM kdeneon/plasma:developer 
RUN sudo apt-get update 
RUN export DEBIAN_FRONTEND=noninteractive
COPY install_dependencies.sh .
RUN sudo chown neon install_dependencies.sh
RUN chmod +x install_dependencies.sh
RUN ./install_dependencies.sh
