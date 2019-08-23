IDIR =./include
CC=g++
CFLAGS=-I$(IDIR)

ODIR =./bin
SDIR =./src
BDIR =./

_INDEXDEPS = argparse.hpp pugixml.hpp
INDEXDEPS = $(patsubst %,$(IDIR)/%,$(_INDEXDEPS))

_INDEXOBJ = indexer.o
INDEXOBJ = $(patsubst %,$(ODIR)/%,$(_INDEXOBJ))

_INDEXBUILDS = Indexer
INDEXBUILDS = $(patsubst %,$(BDIR)/%,$(_INDEXBUILDS))

_QUERYDEPS = argparse.hpp
QUERYDEPS = $(patsubst %,$(IDIR)/%,$(_QUERYDEPS))

_QUERYOBJ = model.o
QUERYOBJ = $(patsubst %,$(ODIR)/%,$(_QUERYOBJ))

_QUERYBUILDS = bm25_run
QUERYBUILDS = $(patsubst %,$(BDIR)/%,$(_QUERYBUILDS))

BUILDS = $(INDEXBUILDS) $(QUERYBUILDS)

$(ODIR)/indexer.o: $(SDIR)/indexer.cpp $(INDEXDEPS)
	mkdir -p bin
	$(CC) -c -o $@ $< $(CFLAGS) -g

$(ODIR)/model.o: $(SDIR)/model.cpp $(QUERYDEPS)
	mkdir -p bin
	$(CC) -c -o $@ $< $(CFLAGS) -g

Indexer: $(INDEXBUILDS)

bm25_run: $(QUERYBUILDS) 

all: $(BUILDS)

$(BDIR)/Indexer: $(INDEXOBJ)
	$(CC) -o $@ $^ $(CFLAGS)

$(BDIR)/bm25_run: $(QUERYOBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o $(BUILDS)
