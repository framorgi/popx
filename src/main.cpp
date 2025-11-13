

#include "popx_app.h"
#include "grid_world.h"
#include "sfml_graphic_engine.h"


#include <memory>

/*int main()
{
    const int windowSize = 800;
    const int cellSize = 4;
    const int gridCount = windowSize / cellSize;

    sf::RenderWindow window(sf::VideoMode(windowSize, windowSize), "POPx");

    // --- Crea la griglia ---
    std::vector<sf::RectangleShape> gridLines;

    // Linee verticali
    for (int i = 0; i <= gridCount; ++i) {
        sf::RectangleShape line(sf::Vector2f(1, windowSize));
        line.setPosition(i * cellSize, 0);
        line.setFillColor(sf::Color(100, 100, 100));
        gridLines.push_back(line);
    }

    // Linee orizzontali
    for (int i = 0; i <= gridCount; ++i) {
        sf::RectangleShape line(sf::Vector2f(windowSize, 1));
        line.setPosition(0, i * cellSize);
        line.setFillColor(sf::Color(100, 100, 100));
        gridLines.push_back(line);
    }

   std::shared_ptr<pop> p = std::make_shared<pop>();
    p->init();
    std::shared_ptr<grid_world> world = std::make_shared<grid_world>(windowSize/cellSize, windowSize/cellSize);

    world->init();
    Position startPos = {75,39};
    p->try_spawn(world, startPos);


    // --- Crea il cerchio ---
    sf::CircleShape circle(cellSize/2); // raggio 40 -> diametro 80, quindi entra nella cella 100x100
    circle.setFillColor(sf::Color::Green);

    int cellX = p->get_position().x; // 
    int cellY =p->get_position().y; // 

    // Calcola la posizione in pixel per centrare il cerchio nella cella
    float circleX = cellX * cellSize + (cellSize / 2) - circle.getRadius();
    float circleY = cellY * cellSize + (cellSize / 2) - circle.getRadius();

    circle.setPosition(circleX, circleY);
    int incx = cellSize;
    int incy = cellSize;
    // --- Loop principale ---
    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        window.clear(sf::Color::Black);

        // Disegna la griglia
        for (auto &line : gridLines)
            //window.draw(line);

        // Disegna il cerchio
        window.draw(circle);

        window.display();
        circleX+=incx;
        if (circleX > windowSize || circleX < 0) 
        {
            incx = -incx;
            if (circleY > windowSize || circleY < 0) incy = -incy;
            circleY+=incy;
        }
       

        circle.setPosition(circleX, circleY);
        
        
        //sf::sleep(sf::milliseconds(1));

    }

    return 0;
}*/
    

int  main ()
{

    std::shared_ptr<console_logger> app_logger = std::make_shared<console_logger>();
    std::shared_ptr<grid_world> app_grid_world = std::make_shared<grid_world>(100, 100, app_logger);
    std::shared_ptr<pops_manager> app_agents_manager = std::make_shared<pops_manager>(app_grid_world, app_logger);
    std::shared_ptr<simulator> app_sim = std::make_shared<simulator>(app_grid_world, app_agents_manager, app_logger);
    std::shared_ptr<sfml_graphic_engine> app_gfx = std::make_shared<sfml_graphic_engine>();
    std::shared_ptr<renderer> app_renderer = std::make_shared<renderer>(app_gfx, app_grid_world);
    popx_app app(app_sim, app_renderer, app_logger);
    app.init();
    app.run();

    return 0;
}