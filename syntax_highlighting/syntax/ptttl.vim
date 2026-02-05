syntax match ptttlOptionSetting "=\s*\zs[0-9]\+\ze"
syntax match ptttlOptionKey "\zs\(d\|o\|b\|f\|v\)\ze\s*="
"syntax match ptttlNote "\zs[0-9]\?[A-Ga-g][Bb\#]\?[0-9]\?\(v\([0-9]+\(-[0-9]+\)\?\)\?\)\?\ze"
syntax match ptttlNote "[0-9]\?[A-Ga-g][Bb\#]\?[0-9]\?\(v\([0-9]+\(-[0-9]+\)\?\)\?\)\?"
syntax region ptttlDescription start="\%^" end="\ze:"
syntax region ptttlComment start="/" end="$"

highlight default link ptttlDescription String
highlight default link ptttlOptionSetting Number
highlight default link ptttlOptionKey Identifier
highlight default link ptttlComment Comment
highlight default link ptttlNote Type
