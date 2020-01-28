FROM pypy:2

MAINTAINER WSServer Project <morten@mortz.dk>

# Application home
ENV HOME /app
ENV DEBIAN_FRONTEND noninteractive
ENV NODE_PATH /usr/local/lib/node_modules/

# make "pypy" available as "python"
RUN ln -s /usr/local/bin/pypy /usr/local/bin/python

# install Autobahn|Testsuite
RUN pip install -U pip && pip install autobahntestsuite

# make volumes for input configuration and output reports
VOLUME /config
VOLUME /reports

WORKDIR /
EXPOSE 9001 9001

# run wstest by default in fuzzingclient mode
CMD ["/usr/local/bin/wstest", "--mode", "fuzzingclient", "--spec", "/config/fuzzingclient.json"]
