syntax match ptttlDescription "\%^\([^:]\|\_s\)*"
syntax match ptttlOptionSetting "\(d\|o\|b\|f\|v\)\s*=\s*[0-9]\+"
syntax match ptttlComment "^[^#]*\zs#.*\ze"

highlight default link ptttlDescription String
highlight default link ptttlOptionSetting Type
highlight default link ptttlComment Comment
