	Etape 1 : mesure temps
		
Temps passé dans la simulation par pas de temps :
-	Sans affichage : 0.0300159
-	Avec affichage : 0.1001452
Temps passé à l’affichage par pas de temps : 0.07
On constate que le temps d’affichage vaut 2.3 fois le temps de simulation et le temps passé dans un pas de temps est bien la somme du temps de simulation et le temps d’affichage.

	Etape 2: simulation_sync_affiche_mpi.cpp
	
Temps passé dans la simulation par pas de temps:
-	0.04071328
On remarque que le temps de simulation augmente par rapport au cas précédent. Avec les 2 processus, il y'a le temps de
communication en message bloquant qui s'ajoute.

	Etape 3 : simulation_async_affiche_mpi.cpp
	
Temps passé dans la simulation par pas de temps:
-	0.0280639
On constate un speedup de 1,45 qui est compréhensible. En effet la communication se fait maintenant par message non bloquant, ce qui permet aux processus de travailler sans s'arreter ce qui permet le gain de temps.

	Etape 4 : simulation_async_omp.cpp
	
Temps passé dans la simulation par pas de temps:
Avec 2 threads dans les boucles "for" de màjStatistique() et 1 thread dans la boucle de temps
-	0.0309134 
Par rapport au cas précédent c'est plus lent. Augmentons le nombre de threads dans la fonction màjStatistique()
Avec 16 threads dans les boucles "for" de màjStatistique() de while et 1 threads dans la boucle de temps)
-	0.0315362
On a pas vraiment d'amélioration.
On va augmenter le nombre threads dans la boucle de temps
Avec 2 threads dans les boucles "for" de màjStatistique() et 2 thread dans la boucle de temps
-	0.0316197
Avec 8 threads dans les boucles "for" de màjStatistique() et 4 thread dans la boucle de temps
-	0.0214047 
Cependant avec 10 threads dans màjStatistique() , resultats differents (courbe.dat est modifié).
On constate une accélaration avec un nombre de threads croissant. Mais à partir de 10 threads on perd en performance.

	Etape 5 : simulation_async_mpi.cpp
	
Temps passé dans la simulation par pas de temps:
-	0.0270652
Dans cette partie, j'ai rencontré beaucoup de problèmes. En effet quelque soit le nombre processus mpi, je ne remarque pas une accélération significative par rapport à la partie simulation_async_affiche_mpi.cpp. Après ce constat je me suis mis à chercher la raison. J'ai fini par me dire que tous les processus n'arrivaient pas à bien éxécuter la boucle de temps dans la simulation. J'ai cherché à resoudre ce problème en vain.

	Etape 6 : simulation_async_mpi_omp.cpp

	
Temps passé dans la simulation par pas de temps:
-	0.0271851


	Bilan:

En analysant les résultats on voit déjà que plus on augmente la taille de la population, plus on observe de meilleurs performance (speedup plus élevé). Pour l'étape 3 simulation et affichage en asynchrone, l'accélération est plus importante si on prend un nombre d'invidus plus grand.
A travers j'ai mieux compris l'utilité de la programmation parallèle. D'un code séquentiel sur une seule machine, on
peut parvenir à distribuer le travail sur plusieurs processus en parallèle voir sur plusieurs machines distinctes; 
et cela avec un gain de performance.
J'ai également chercher à connaitre les composants de mon ordinateur. Cependant j'ai l'impression qu'avec le WSL que j'ai utilisé pour mon projet, les résultats étaient parfois un peu entachés d'incohérences. En effet, d'un jour à l'autre je pouvais avoir des accélérations différentes. 
Ce projet m'a poussé à visiter plusieurs forums sur la programation parallèle et j'ai pris conscience de la communanauté et de quelques applications.
