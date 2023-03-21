@set dir=%CD%
@mkdir %out%
cd %out%
@echo msys=%msys% >config
@echo include=%include% >>config
@echo lib=%lib% >>config
@echo root=..>>config
@copy %dir%\makefile
@make >%dir%\build.log 2>%dir%\build_error.log
@cd %dir%
