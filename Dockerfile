from jonaski/opensuse:tumbleweed

run mkdir -p /usr/src/app
workdir /usr/src/app
copy . /usr/src/app
