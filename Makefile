run:
	rm -Rf $$(pwd)/lab/shared
	mkdir -p $$(pwd)/lab/shared
	cp -r p4src $$(pwd)/lab/shared
	kathara lstart -d $$(pwd)/lab

clean:
	kathara lclean -d $$(pwd)/lab

experiments_emulator:
	cd experiments && python3 experiment_n_path.py results/ 6 5

plot_experiments_emulator:
	cd experiments && python3 plot.py results/ figures/