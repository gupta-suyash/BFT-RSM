FILE = main
$(FILE).pdf: $(FILE).tex intro.tex header.tex prelim.tex algo-nc.tex primitive.tex eval.tex related.tex conclusion.tex graphs.tex proofs.tex
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
