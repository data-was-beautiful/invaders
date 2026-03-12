### Compiling

gcc -O2 -o alien_invasion alien_invasion.c $(sdl2-config --cflags --libs) -lm ./alien_invasion