FILE = bftcomm
$(FILE).pdf: $(FILE).tex intro.tex prelim.tex algo.tex failures.tex motivate.tex formalization.tex bridges.tex threat_model.tex
	pdflatex $(FILE)

newcite: refined.bib $(FILE).pdf 
	bibtex $(FILE)
	pdflatex $(FILE)
	pdflatex $(FILE)
	
force:
	pdflatex $(FILE)
	bibtex $(FILE)
	pdflatex $(FILE)
	pdflatex $(FILE)
	
clean:
	rm -f $(FILE).aux $(FILE).bbl $(FILE).blg $(FILE).dvi $(FILE).log $(FILE).pdf $(FILE).ps $(FILE).out
