.PHONY: clean All

All:
	@echo "----------Building project:[ qwertycoin - Debug ]----------"
	@"$(MAKE)" -f  "qwertycoin.mk"
clean:
	@echo "----------Cleaning project:[ qwertycoin - Debug ]----------"
	@"$(MAKE)" -f  "qwertycoin.mk" clean
