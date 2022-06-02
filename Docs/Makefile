FILE = bftcomm
$(FILE).pdf: $(FILE).tex intro.tex prelim.tex algo.tex failures.tex
	pdflatex $(FILE)

newcite:  $(FILE).pdf 
	pdflatex $(FILE)
	pdflatex $(FILE)
	
force:
	pdflatex $(FILE)
	pdflatex $(FILE)
	pdflatex $(FILE)
	
clean:
	rm -f $(FILE).aux $(FILE).bbl $(FILE).blg $(FILE).dvi $(FILE).log $(FILE).pdf $(FILE).ps $(FILE).out
