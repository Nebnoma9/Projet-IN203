#include <cstdlib>
#include <random>
#include <iostream>
#include <fstream>
#include "contexte.hpp"
#include "individu.hpp"
#include "graphisme/src/SDL2/sdl2.hpp"

#include <chrono>
#include<mpi.h>
    
#define COUNT 1023
#define COUNT_0 100172
#define COUNT_1 200256
#define COUNT_2 400056

void màjStatistique( épidémie::Grille& grille, std::vector<épidémie::Individu> const& individus )
{
    #pragma omp parallel for schedule(dynamic) num_threads(2)
    for ( auto& statistique : grille.getStatistiques() )
    {
        statistique.nombre_contaminant_grippé_et_contaminé_par_agent = 0;
        statistique.nombre_contaminant_seulement_contaminé_par_agent = 0;
        statistique.nombre_contaminant_seulement_grippé              = 0;
    }
    auto [largeur,hauteur] = grille.dimension();
    auto& statistiques = grille.getStatistiques();

    #pragma omp parallel for schedule(dynamic) num_threads(2)
    for ( auto const& personne : individus )
    {
        auto pos = personne.position();

        std::size_t index = pos.x + pos.y * largeur;
        if (personne.aGrippeContagieuse() )
        {
            if (personne.aAgentPathogèneContagieux())
            {
                statistiques[index].nombre_contaminant_grippé_et_contaminé_par_agent += 1;
            }
            else 
            {
                statistiques[index].nombre_contaminant_seulement_grippé += 1;
            }
        }
        else
        {
            if (personne.aAgentPathogèneContagieux())
            {
                statistiques[index].nombre_contaminant_seulement_contaminé_par_agent += 1;
            }
        }
    }
}


void afficheSimulation(sdl2::window& écran, std::array<int,2> dim,
                std::vector<épidémie::Grille::StatistiqueParCase> const& grill_stat, std::size_t jour)
{
    auto [largeur_écran,hauteur_écran] = écran.dimensions();
    auto [largeur_grille,hauteur_grille] = dim;
    auto const& statistiques = grill_stat;
    sdl2::font fonte_texte("./graphisme/src/data/Lato-Thin.ttf", 18);
    écran.cls({0x00,0x00,0x00});
    // Affichage de la grille :
    std::uint16_t stepX = largeur_écran/largeur_grille;
    unsigned short stepY = (hauteur_écran-50)/hauteur_grille;
    double factor = 255./15.;

    for ( unsigned short i = 0; i < largeur_grille; ++i )
    {
        for (unsigned short j = 0; j < hauteur_grille; ++j )
        {
            auto const& stat = statistiques[i+j*largeur_grille];
            int valueGrippe = stat.nombre_contaminant_grippé_et_contaminé_par_agent+stat.nombre_contaminant_seulement_grippé;
            int valueAgent  = stat.nombre_contaminant_grippé_et_contaminé_par_agent+stat.nombre_contaminant_seulement_contaminé_par_agent;
            std::uint16_t origx = i*stepX;
            std::uint16_t origy = j*stepY;
            std::uint8_t red = valueGrippe > 0 ? 127+std::uint8_t(std::min(128., 0.5*factor*valueGrippe)) : 0;
            std::uint8_t green = std::uint8_t(std::min(255., factor*valueAgent));
            std::uint8_t blue= std::uint8_t(std::min(255., factor*valueAgent ));
            écran << sdl2::rectangle({origx,origy}, {stepX,stepY}, {red, green,blue}, true);
        }
    }

    écran << sdl2::texte("Carte population grippée", fonte_texte, écran, {0xFF,0xFF,0xFF,0xFF}).at(largeur_écran/2, hauteur_écran-20);
    écran << sdl2::texte(std::string("Jour : ") + std::to_string(jour), fonte_texte, écran, {0xFF,0xFF,0xFF,0xFF}).at(0,hauteur_écran-20);
    écran << sdl2::flush;
}



void simulation(bool affiche,int rank,MPI_Comm comm)
{
    
    int blockcount[3]={1,1,1};
    MPI_Aint offsets[3] = {offsetof(épidémie::Grille::StatistiqueParCase, nombre_contaminant_seulement_grippé), 
                        offsetof(épidémie::Grille::StatistiqueParCase, nombre_contaminant_seulement_contaminé_par_agent), 
                        offsetof(épidémie::Grille::StatistiqueParCase, nombre_contaminant_grippé_et_contaminé_par_agent)};
    MPI_Datatype dataType[3] = {MPI_INT, MPI_INT, MPI_INT};
    MPI_Datatype StatistiqueParCaseT;
    MPI_Type_create_struct(3, blockcount, offsets, dataType, &StatistiqueParCaseT);
    MPI_Type_commit(&StatistiqueParCaseT);
    MPI_Status status;
    MPI_Request request;

    unsigned int graine_aléatoire = 1;
    std::uniform_real_distribution<double> porteur_pathogène(0.,1.);


    épidémie::ContexteGlobal contexte;
    // contexte.déplacement_maximal = 1; <= Si on veut moins de brassage
    //contexte.taux_population = 400'000;
    //contexte.taux_population = 1'000;
    //contexte.taux_population = 200'000;
    contexte.interactions.β = 60.;
    std::vector<épidémie::Individu> population;
    population.reserve(contexte.taux_population);
    épidémie::Grille grille{contexte.taux_population};

    auto [largeur_grille,hauteur_grille] = grille.dimension();
    // L'agent pathogène n'évolue pas et reste donc constant...
    épidémie::AgentPathogène agent(graine_aléatoire++);
    // Initialisation de la population initiale :
    for (std::size_t i = 0; i < contexte.taux_population; ++i )
    {
        std::default_random_engine motor(100*(i+1));
        population.emplace_back(graine_aléatoire++, contexte.espérance_de_vie, contexte.déplacement_maximal);
        population.back().setPosition(largeur_grille, hauteur_grille);
        if (porteur_pathogène(motor) < 0.2)
        {
            population.back().estContaminé(agent);   
        }
    }

    std::size_t jours_écoulés = 0;
    int         jour_apparition_grippe = 0;
    int         nombre_immunisés_grippe= (contexte.taux_population*23)/100;
    

    bool quitting = false;

    std::ofstream output("Courbe.dat");
    output << "# jours_écoulés \t nombreTotalContaminésGrippe \t nombreTotalContaminésAgentPathogène()" << std::endl;

    épidémie::Grippe grippe(0);

  
    std::cout <<"Début boucle épidémie" << std::endl << std::flush;
   
    while (!quitting)
    {   
        // mesure temps
        std::chrono::time_point<std::chrono::system_clock> start, end, end2;
        start = std::chrono::system_clock::now();

        
        MPI_Irecv(&quitting,1, MPI::BOOL, 0, 0, MPI_COMM_WORLD, &request); //1

        if (jours_écoulés%365 == 0)// Si le premier Octobre (début de l'année pour l'épidémie ;-) )
        {
            grippe = épidémie::Grippe(jours_écoulés/365);
            jour_apparition_grippe = grippe.dateCalculImportationGrippe();
            grippe.calculNouveauTauxTransmission();
            // 23% des gens sont immunisés. On prend les 23% premiers
            for ( int ipersonne = 0; ipersonne < nombre_immunisés_grippe; ++ipersonne)
            {
                population[ipersonne].devientImmuniséGrippe();
            }
            for ( int ipersonne = nombre_immunisés_grippe; ipersonne < int(contexte.taux_population); ++ipersonne )
            {
                population[ipersonne].redevientSensibleGrippe();
            }
        }
        if (jours_écoulés%365 == std::size_t(jour_apparition_grippe))
        {
            for (int ipersonne = nombre_immunisés_grippe; ipersonne < nombre_immunisés_grippe + 25; ++ipersonne )
            {
                population[ipersonne].estContaminé(grippe);
            }
        }
        // Mise à jour des statistiques pour les cases de la grille :
        màjStatistique(grille, population);
        // On parcout la population pour voir qui est contaminé et qui ne l'est pas, d'abord pour la grippe puis pour l'agent pathogène
        std::size_t compteur_grippe = 0, compteur_agent = 0, mouru = 0;
        #pragma omp parallel for schedule(dynamic) num_threads(2) //reduction(+:compteur_grippe) reduction(+:compteur_agent) reduction(+:mouru)    
        for ( auto& personne : population )
        {
            // On vérifie si il n'y a pas de personne qui veillissent de veillesse et on génère une nouvelle personne si c'est le cas.
           #pragma omp critical
            {
                if (personne.doitMourir())
                {
                    mouru++;
                    unsigned nouvelle_graine = jours_écoulés + personne.position().x*personne.position().y;
                    personne = épidémie::Individu(nouvelle_graine, contexte.espérance_de_vie, contexte.déplacement_maximal);
                    personne.setPosition(largeur_grille, hauteur_grille);
                }
            }
            #pragma omp critical 
            {
                if (personne.testContaminationGrippe(grille, contexte.interactions, grippe, agent))
                {
                    compteur_grippe ++;
                    personne.estContaminé(grippe);
                }
            }
            #pragma omp critical
            {
                if (personne.testContaminationAgent(grille, agent))
                {
                    compteur_agent ++;
                    personne.estContaminé(agent);
                }
            }
             #pragma omp critical
            {
                personne.veillirDUnJour();
            }
            personne.seDéplace(grille);
        }

      

        unsigned long jours_écoulé = jours_écoulés;
        auto dim = grille.dimension();
        auto grill_stat = grille.getStatistiques();

        MPI_Isend(&jours_écoulé,1, MPI::UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD,&request);  
        MPI_Isend(&dim, 2, MPI_INT, 0, 0, MPI_COMM_WORLD,&request);  
        MPI_Isend(grill_stat.data(), COUNT_0, StatistiqueParCaseT, 0, 0, MPI_COMM_WORLD,&request);
      
        MPI_Wait(&request,&status);
       
        //if (affiche) afficheSimulation(écran, grille, jours_écoulés);

        /*std::cout << jours_écoulés << "\t" << grille.nombreTotalContaminésGrippe() << "\t"
                  << grille.nombreTotalContaminésAgentPathogène() << std::endl;*/

        output << jours_écoulés << "\t" << grille.nombreTotalContaminésGrippe() << "\t"
               << grille.nombreTotalContaminésAgentPathogène() << std::endl;
        jours_écoulés += 1;


        
        end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = end-start;
        std::cout <<"Temps pas simul: " << elapsed_seconds.count() << std::endl;
        
        
    }// Fin boucle temporelle
    output.close();

}

int main(int argc, char* argv[])
{
   
    // parse command-line
    bool affiche = true;
    for (int i=0; i<argc; i++) {
      std::cout << i << " " << argv[i] << "\n";
      if (std::string("-nw") == argv[i]) affiche = false;
    }

   
    
    sdl2::init(argc, argv);
    {
        MPI_Init(&argc, &argv);
        int size, rank,new_rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &size);
         MPI_Comm comm;
        if(rank==0)
            MPI_Comm_split(MPI_COMM_WORLD,0,0,&comm);
        else
            MPI_Comm_split(MPI_COMM_WORLD,1,rank,&comm);
        MPI_Comm_rank(comm, &new_rank );


        if(rank==0)
        {
            constexpr const unsigned int largeur_écran = 1280, hauteur_écran = 720;
            sdl2::window écran("Simulation épidémie de grippe", {largeur_écran,hauteur_écran});

            int blockcount[3]={1,1,1};
            MPI_Aint offsets[3] = {offsetof(épidémie::Grille::StatistiqueParCase, nombre_contaminant_seulement_grippé), 
            offsetof(épidémie::Grille::StatistiqueParCase, nombre_contaminant_seulement_contaminé_par_agent), 
            offsetof(épidémie::Grille::StatistiqueParCase, nombre_contaminant_grippé_et_contaminé_par_agent)};
            MPI_Datatype dataType[3] = {MPI_INT, MPI_INT, MPI_INT};
            MPI_Datatype StatistiqueParCaseT;
            MPI_Type_create_struct(3, blockcount, offsets, dataType, &StatistiqueParCaseT);
            MPI_Type_commit(&StatistiqueParCaseT);
            MPI_Status status;
            MPI_Request request;

            sdl2::event_queue queue;

            bool quitting = false;
            int flag1=0, flag2=0, flag3=0;
            
            while(!quitting)
            {
               /*  // mesure temps
                std::chrono::time_point<std::chrono::system_clock> start, end, end2;
                start = std::chrono::system_clock::now();*/

                auto events = queue.pull_events();
                for ( const auto& e : events)
                {
                    if (e->kind_of_event() == sdl2::event::quit)
                        quitting = true;
                }
                MPI_Isend(&quitting,1,MPI::BOOL,1,0,MPI_COMM_WORLD,&request); //1

                unsigned long jours_écoulé;
                std::array<int,2> dim;
                std::vector<épidémie::Grille::StatistiqueParCase> grill_stat(COUNT_0);
                //épidémie::Grille::StatistiqueParCase buf;
                for(int i=0;i<1;i++)
                {
                while (!flag1)
                {
                    MPI_Iprobe( 1, 0, MPI_COMM_WORLD, &flag1, &status );  
                   //  std::cout<<"i'm there 1 \n";
                }
                MPI_Recv(&jours_écoulé,1,MPI::UNSIGNED_LONG,1,0,MPI_COMM_WORLD,&status); 
                flag1 = 0;//3
                while (!flag2)
                {
                    MPI_Iprobe( 1, 0, MPI_COMM_WORLD, &flag2, &status );
                     //std::cout<<"i'm there  2\n";
                }
                MPI_Recv(&dim,2,MPI_INT,1,0,MPI_COMM_WORLD,&status); //5
                flag2 = 0;
                while (!flag3)
                {
                    MPI_Iprobe( 1, 0, MPI_COMM_WORLD, &flag3, &status );
                     //std::cout<<"i'm there  2\n";
                }
                MPI_Recv(grill_stat.data(),COUNT_0,StatistiqueParCaseT,1,0,MPI_COMM_WORLD,&status); //6
                flag3 = 0;
                }
            //#############################################################################################################
            //##    Affichage des résultats pour le temps  actuel
            //#############################################################################################################

                if (affiche) afficheSimulation(écran, dim, grill_stat, (size_t) jours_écoulé);
                
                MPI_Wait(&request,&status);
               /* end = std::chrono::system_clock::now();
                std::chrono::duration<double> elapsed_seconds = end-start;
                std::cout <<"Temps pas affich: " << elapsed_seconds.count() << std::endl;*/
            }
         
        }
        else
        {
            simulation(affiche,new_rank,comm);
        }
        MPI_Finalize();
    }
    sdl2::finalize();
    return EXIT_SUCCESS;
}
