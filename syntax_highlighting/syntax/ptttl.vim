syntax match ptttlOptionSetting "=\s*\zs[0-9]\+\ze"
syntax match ptttlNote "[0-9]\?[A-Ga-g][Bb\#]\?[0-9]\?\(v\([0-9]+\(-[0-9]+\)\?\)\?\)\?"
syntax match ptttlOptionKey "\zs\(d\|o\|b\|f\|v\)\ze\s*="
syntax match ptttlPause "\(1\|2\|4\|8\|16\|32\)p"
syntax region ptttlComment start="/" end="$"
syntax region ptttlDescription start="\%^" end="\ze:" contains=ptttlComment

highlight default link ptttlDescription String
highlight default link ptttlOptionSetting Number
highlight default link ptttlOptionKey Identifier
highlight default link ptttlComment Comment
highlight default link ptttlNote Type
highlight default link ptttlPause PreProc
