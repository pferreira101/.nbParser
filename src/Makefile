Notebook: main.c
	gcc main.c -o notebook
	mkdir -p /tmp/notebook


doc:	
	      doxygen -g && doxygen
	mv    Doxyfile ../doc/Doxyfile
	mv    latex ../doc/latex
	mv    html ../doc/html
	

clean: 
	rm -f notebook
	rm -f Doxyfile.bak
	rm -rf /tmp/notebook
	rm -rf ../doc/latex
	rm -rf ../doc/html
	rm -f ../doc/Doxyfile
