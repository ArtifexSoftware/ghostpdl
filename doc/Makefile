DOCS=ghostpcl.dvi ghostpcl.pdf ghostpcl.txt

# latex 2 dvi
%.dvi : %.tex
	latex $<

# latex 2 pdf
%.pdf : %.tex
	pdflatex $<

# pdf 2 text
%.txt : %.pdf
	pdftotext $<

all: $(DOCS)

clean:
	rm -f $(DOCS)
