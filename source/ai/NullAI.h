/*
 *  Copyright (C) 2011-2014  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NULLAI_H
#define NULLAI_H

#include "ai/AIFactory.h"
#include "ai/BaseAI.h"

//! \brief An AI that does nothing. Useful for neutral creatures and teams.
class NullAI : public BaseAI
{

public:
    NullAI(GameMap& gameMap, Player& player, const std::string& parameters = std::string());

    virtual bool doTurn(double frameTime);
private:
    //TODO: add macro that registers and implements a getname function.
    static AIFactoryRegister<NullAI> reg;
};

#endif // NULLAI_H