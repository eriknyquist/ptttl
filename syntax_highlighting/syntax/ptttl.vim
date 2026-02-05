syntax match ptttlNote "[0-9]\?[A-Ga-g][Bb#]\?[0-9]\?\(v\([0-9]\+\(-[0-9]\+\)\?\)\?\)\?"
syntax match ptttlOptionSetting "=\s*\zs\d\+"
syntax match ptttlOptionKey "[dobfv]\ze\s*="
syntax match ptttlPipe "|"
syntax match ptttlComma ","
syntax match ptttlColon ":"
syntax match ptttlSemicolon ";"
syntax region ptttlComment start="/" end="$"
syntax region ptttlDescription start="\%^" end="\ze:" contains=ptttlComment

highlight default link ptttlDescription String
highlight default link ptttlComment Comment
highlight default link ptttlNote Type
highlight default link ptttlPipe Operator
highlight default link ptttlSemicolon Include
highlight default link ptttlComma Special
highlight default link ptttlColon Exception
highlight default link ptttlOptionSetting Number
highlight default link ptttlOptionKey Identifier
