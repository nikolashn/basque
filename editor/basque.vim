" Vim syntax file
" Language: Basque
" Maintainer: nikolashn
" Latest Revision: 2022 Jul 30
" Usage instructions:

" Put this file in .vim/syntax/basque.vim,
" and the following line in your .vimrc
" au BufRead,BufNewFile *.ba set filetype=basque

if exists("b:current_syntax")
	finish
endif

syn keyword baTodo contained TODO FIXME XXX NOTE
syn region baMLComment start="#{" end="}#" contains=baTodo
syn match baComment "#\([^{].*\)\?$" contains=baTodo

syn match baNumber "\<-\?[0-9_]\+\(u\|U\)\?\>"
syn match baNumber "\<-\?0x[0-9a-fA-F_]\+\(u\|U\)\?\>"
syn match baNumber "\<-\?0o[0-7_]\+\(u\|U\)\?\>"
syn match baNumber "\<-\?0b[01_]\+\(u\|U\)\?\>"
syn keyword baNumber garbage

syn region baString start='"' end='"' skip='\\"' contains=baEscape
syn match baEscape contained '\\\(["ntvfrb0\n\\]\|\'\|\(x\o\x\)\)'

syn keyword baConditional if elif else
syn keyword baRepeat while iter
syn keyword baStatement write break goto return include exit
syn keyword baType u64 i64 u8 i8 void
syn keyword baOperator lengthof

syn match baLabel "\<[a-zA-Z_][a-zA-Z0-9_]*:"

hi def link baComment Comment
hi def link baMLComment Comment
hi def link baNumber Number
hi def link baEscape Special
hi def link baString String
hi def link baConditional Conditional
hi def link baRepeat Repeat
hi def link baStatement Statement
hi def link baLabel Label
hi def link baType Type
hi def link baTodo Todo
hi def link baOperator Operator

let b:current_syntax = "basque"

