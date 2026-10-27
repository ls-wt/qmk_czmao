define GEN
$1/force:
$1/cflags.txt: $1/force
	$(shell mkdir -p $1)
	$(file >$1/cflags.txt,$$($1_CFLAGS))
	@echo "wrote cflags.txt, content:"; cat $1/cflags.txt
endef

OUTPUT := .build/obj_x
$(OUTPUT)_CFLAGS := -DFOO='bar' -DBAZ="qux"
OUTPUTS := .build/obj_x
$(foreach OUTPUT,$(OUTPUTS),$(eval $(call GEN,$(OUTPUT))))

force:
