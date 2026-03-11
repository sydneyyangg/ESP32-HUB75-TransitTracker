FROM python:3.11-slim

RUN pip install platformio

WORKDIR /project
# copy takes 2 arguments: the first is src, which is from the directory the dockerfile is in
# the second is dest, which is from workdir

COPY platformio.ini /project/

# take adv of caching!
RUN pio pkg install

COPY . .
RUN pio run -e esp32