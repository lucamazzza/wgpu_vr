# Tell latexmk to use XeLaTeX
$pdf_mode = 5; 
$xelatex = 'xelatex -shell-escape -interaction=nonstopmode -synctex=1 %O %S';

# Tell latexmk how to handle the glossaries package
add_cus_dep('glo', 'gls', 0, 'makeglossaries');
add_cus_dep('acn', 'acr', 0, 'makeglossaries');
sub makeglossaries {
    system("makeglossaries '$_[0]'");
}
