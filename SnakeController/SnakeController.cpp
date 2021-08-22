#include "SnakeController.hpp"

#include <algorithm>
#include <sstream>

#include "EventT.hpp"
#include "IPort.hpp"

namespace Snake
{
ConfigurationError::ConfigurationError()
    : std::logic_error("Bad configuration of Snake::Controller.")
{}

UnexpectedEventException::UnexpectedEventException()
    : std::runtime_error("Unexpected event received!")
{}

Controller::Controller(IPort& p_displayPort, IPort& p_foodPort, IPort& p_scorePort, std::string const& p_config)
    : m_displayPort(p_displayPort),
      m_foodPort(p_foodPort),
      m_scorePort(p_scorePort)
{
    std::istringstream istr(p_config);
    char w, f, s, d;

    int width, height, length;
    int foodX, foodY;
    istr >> w >> width >> height >> f >> foodX >> foodY >> s;

    if (w == 'W' and f == 'F' and s == 'S') {
        m_mapDimension = std::make_pair(width, height);
        m_foodPosition = std::make_pair(foodX, foodY);

        istr >> d;
        switch (d) {
            case 'U':
                m_currentDirection = Direction_UP;
                break;
            case 'D':
                m_currentDirection = Direction_DOWN;
                break;
            case 'L':
                m_currentDirection = Direction_LEFT;
                break;
            case 'R':
                m_currentDirection = Direction_RIGHT;
                break;
            default:
                throw ConfigurationError();
        }
        istr >> length;

        while (length) {
            Segment seg;
            istr >> seg.x >> seg.y;
            seg.ttl = length--;

            m_segments.push_back(seg);
        }
    } else {
        throw ConfigurationError();
    }
}

bool Controller::is_head_crashed(const Segment& newHead){
    for (auto segment : m_segments) {
        if (segment.x == newHead.x and segment.y == newHead.y) {
            m_scorePort.send(std::make_unique<EventT<LooseInd>>());
            return true;
            break;
        }
    }
    return false;
}

void Controller::grab_food(const Segment& newHead, bool& lost){
    if (std::make_pair(newHead.x, newHead.y) == m_foodPosition) {
        m_scorePort.send(std::make_unique<EventT<ScoreInd>>());
        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
    } else if (newHead.x < 0 or newHead.y < 0 or
               newHead.x >= m_mapDimension.first or
               newHead.y >= m_mapDimension.second) {
        m_scorePort.send(std::make_unique<EventT<LooseInd>>());
        lost = true;
    } else {
        for (auto &segment : m_segments) {
            if (not --segment.ttl) {
                DisplayInd l_evt;
                l_evt.x = segment.x;
                l_evt.y = segment.y;
                l_evt.value = Cell_FREE;

                m_displayPort.send(std::make_unique<EventT<DisplayInd>>(l_evt));
            }
        }
    }
}

void Controller::delete_one_cell_from_tail(){
    m_segments.erase(
        std::remove_if(
            m_segments.begin(),
            m_segments.end(),
            [](auto const& segment){ return not (segment.ttl > 0); }),
        m_segments.end());
}

void Controller::move_one_cell(const Segment& newHead){
    m_segments.push_front(newHead);
    DisplayInd placeNewHead;
    placeNewHead.x = newHead.x;
    placeNewHead.y = newHead.y;
    placeNewHead.value = Cell_SNAKE;

    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewHead));

    delete_one_cell_from_tail();
}

void Controller::error_handle(std::unique_ptr<Event> e){
    try {
        auto direction = dynamic_cast<EventT<DirectionInd> const&>(*e)->direction;

        if ((m_currentDirection & 0b01) != (direction & 0b01)) {
            m_currentDirection = direction;
        }
    } catch (std::bad_cast&) {
        try {
            auto receivedFood = *dynamic_cast<EventT<FoodInd> const&>(*e);

            bool requestedFoodCollidedWithSnake = is_new_food_colidate(receivedFood);

            clear_and_create_new_food(receivedFood, requestedFoodCollidedWithSnake);

            m_foodPosition = std::make_pair(receivedFood.x, receivedFood.y);

        } catch (std::bad_cast&) {
            try {
                auto requestedFood = *dynamic_cast<EventT<FoodResp> const&>(*e);

                bool requestedFoodCollidedWithSnake = false;

                for (auto const& segment : m_segments) {
                    if (segment.x == requestedFood.x and segment.y == requestedFood.y) {
                        requestedFoodCollidedWithSnake = true;
                        break;
                    }
                }

                if (requestedFoodCollidedWithSnake) {
                    m_foodPort.send(std::make_unique<EventT<FoodReq>>());
                } else {
                    DisplayInd placeNewFood;
                    placeNewFood.x = requestedFood.x;
                    placeNewFood.y = requestedFood.y;
                    placeNewFood.value = Cell_FOOD;
                    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewFood));
                }

                m_foodPosition = std::make_pair(requestedFood.x, requestedFood.y);
            } catch (std::bad_cast&) {
                throw UnexpectedEventException();
            }
        }
    }
}
void Controller::clear_old_food(const FoodInd& receivedFood){
    DisplayInd clearOldFood;
    clearOldFood.x = m_foodPosition.first;
    clearOldFood.y = m_foodPosition.second;
    clearOldFood.value = Cell_FREE;
    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(clearOldFood));
}

void Controller::place_new_food(const FoodInd& receivedFood){
    DisplayInd placeNewFood;
    placeNewFood.x = receivedFood.x;
    placeNewFood.y = receivedFood.y;
    placeNewFood.value = Cell_FOOD;
    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewFood));
}

void Controller::clear_and_create_new_food(const FoodInd& receivedFood, bool requestedFoodCollidedWithSnake) {
    if (requestedFoodCollidedWithSnake) {
        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
    } else {
        clear_old_food(receivedFood);
        place_new_food(receivedFood);
    }
}

bool Controller::is_new_food_colidate(const FoodInd& receivedFood) {
    for (auto const& segment : m_segments) {
        if (segment.x == receivedFood.x and segment.y == receivedFood.y) {
            return true;
            break;
        }
    }
    return false;
}

Controller::Segment Controller::get_new_head(){
    Segment const& currentHead = m_segments.front();

    Segment newHead;
    newHead.x = currentHead.x + ((m_currentDirection & 0b01) ? (m_currentDirection & 0b10) ? 1 : -1 : 0);
    newHead.y = currentHead.y + (not (m_currentDirection & 0b01) ? (m_currentDirection & 0b10) ? 1 : -1 : 0);
    newHead.ttl = currentHead.ttl;

    return newHead;
}

void Controller::receive(std::unique_ptr<Event> e)
{
    try {
        auto const& timerEvent = *dynamic_cast<EventT<TimeoutInd> const&>(*e);

        Segment newHead = get_new_head();

        bool lost = is_head_crashed(newHead);

        if (not lost) {
            grab_food(newHead, lost);
        }

        if (not lost) {
           move_one_cell(newHead);
        }

    } catch (std::bad_cast&) {
       error_handle(std::move(e));
    }
}

} // namespace Snake
