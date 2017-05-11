PROG = coffee_httpd
SOURCES = $(PROG).c mongoose.c
CFLAGS = -Ofast -W -Wall -I../.. $(CFLAGS_EXTRA)
# CFLAGS = -fsanitize=leak -fsanitize=undefined -fsanitize=bounds -O1 -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -W -Wall -I../.. $(CFLAGS_EXTRA)

all: $(PROG)

$(PROG): $(SOURCES)
	$(CC) $(SOURCES) -o $@ $(CFLAGS)

clean:
	rm -f $(PROG)
