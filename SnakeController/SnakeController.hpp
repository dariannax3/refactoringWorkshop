#pragma once

#include <list>
#include <memory>

#include "IEventHandler.hpp"
#include "SnakeInterface.hpp"
#include <stdexcept>

class Event;
class IPort;

namespace Snake
{
struct ConfigurationError : std::logic_error
{
    ConfigurationError();
};

struct UnexpectedEventException : std::runtime_error
{
    UnexpectedEventException();
};

class Controller : public IEventHandler
{
public:
    Controller(IPort& p_displayPort, IPort& p_foodPort, IPort& p_scorePort, std::string const& p_config);

    Controller(Controller const& p_rhs) = delete;
    Controller& operator=(Controller const& p_rhs) = delete;

    void receive(std::unique_ptr<Event> e) override;

private:
    struct Segment
    {
        int x;
        int y;
        int ttl;
    };

    bool is_head_crashed(const Segment& newHead);
    void grab_food(const Segment& newHead, bool& lost);
    void move_one_cell(const Segment& newHead);
    void error_handle(std::unique_ptr<Event> e);
    bool is_new_food_colidate(const FoodInd& receivedFood);
    void clear_and_create_new_food(const FoodInd& receivedFood, bool requestedFoodCollidedWithSnake);
    Controller::Segment get_new_head();
    void clear_old_food(const FoodInd& receivedFood);
    void place_new_food(const FoodInd& receivedFood);
    void delete_one_cell_from_tail();


    IPort& m_displayPort;
    IPort& m_foodPort;
    IPort& m_scorePort;

    std::pair<int, int> m_mapDimension;
    std::pair<int, int> m_foodPosition;

    Direction m_currentDirection;
    std::list<Segment> m_segments;
};

} // namespace Snake
