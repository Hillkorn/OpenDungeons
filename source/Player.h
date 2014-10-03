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

#ifndef PLAYER_H
#define PLAYER_H

#include "Trap.h"
#include "Room.h"

#include <string>
#include <vector>

class Seat;
class Creature;

/*! \brief The player cleass contains information about a human, or computer, player in the game.
 *
 * When a new player joins a game being hosted on a server the server will
 * allocate a new Player structure and fill it in with the appropriate values.
 * Its relevant information will then be sent to the other players in the game
 * so they are aware of its presence.  In the future if we decide to do a
 * single player game, thiis is where the computer driven strategy AI
 * calculations will take place.
 */
class Player
{
public:
    Player();

    const std::string& getNick() const
    { return mNickname; }

    Seat* getSeat()
    { return mSeat; }

    const Seat* getSeat() const
    { return mSeat; }

    void setNick (const std::string& nick)
    { mNickname = nick; }

    void setSeat(Seat* seat)
    { mSeat = seat; }

    //! \brief A simple accessor function to return the number of creatures
    //! this player is holding in his/her hand that belongs to seat seat.
    //! If seat is NULL, then returns the total number of creatures
    unsigned int numCreaturesInHand(const Seat* seat = NULL) const;

    //! \brief A simple accessor function to return a pointer to the i'th creature in the players hand.
    Creature *getCreatureInHand(int i);
    const Creature* getCreatureInHand(int i) const;

    /*! \brief Check to see if it is the user or another player picking up the creature and act accordingly.
     *
     * This function takes care of all of the operations required for a player to
     * pick up a creature.  If the player is the user we need to move the creature
     * oncreen to the "hand" as well as add the creature to the list of creatures
     * in our own hand, this is done by setting moveToHand to true.  If move to
     * hand is false we just hide the creature (and stop its AI, etc.), rather than
     * making it follow the cursor.
     */
    void pickUpCreature(Creature *c);

    //! \brief Check to see the first creatureInHand can be dropped on Tile t and do so if possible.
    bool isDropCreaturePossible(Tile *t, unsigned int index = 0, bool isEditorMode = false);

    //! \brief Drops the creature on tile t. Returns the dropped creature
    Creature* dropCreature(Tile *t, unsigned int index = 0);

    void rotateCreaturesInHand(int n);

    //! \brief Clears all creatures that a player might have in his hand
    void clearCreatureInHand();

    inline void setGameMap(GameMap* gameMap)
    { mGameMap = gameMap; }

    inline bool getHasAI() const
    { return mHasAI; }

    inline void setHasAI(bool hasAI)
    { mHasAI = hasAI; }

    inline const std::vector<Creature*>& getCreaturesInHand()
    { return mCreaturesInHand; }

    inline const Room::RoomType getNewRoomType()
    { return mNewRoomType; }

    inline void setNewRoomType(Room::RoomType newRoomType)
    { mNewRoomType = newRoomType; }

    inline const Trap::TrapType getNewTrapType() const
    { return mNewTrapType; }

    inline void setNewTrapType(Trap::TrapType newTrapType)
    { mNewTrapType = newTrapType; }

    inline float getFightingTime() const
    { return mFightingTime; }

    inline void setFightingTime(float fightingTime)
    { mFightingTime = fightingTime; }

private:
    //! \brief Room or trap tile type the player is currently willing to place on map.
    Room::RoomType mNewRoomType;
    Trap::TrapType mNewTrapType;

    GameMap* mGameMap;
    Seat *mSeat;

    //! \brief The nickname used in chat, etc.
    std::string mNickname;

    //! \brief The creature the player has got in hand.
    std::vector<Creature*> mCreaturesInHand;

    //! True: player is human. False: player is a computer.
    bool mHasAI;

    //! \brief This counter tells for how much time is left before considering
    //! the player to be out of struggle.
    //! When > 0, the player is considered attacking or being attacked.
    //! This member is used to trigger the calm or fighting music when incarnating
    //! the local player.
    float mFightingTime;

    //! \brief A simple mutator function to put the given creature into the player's hand,
    //! note this should NOT be called directly for creatures on the map,
    //! for that you should use pickUpCreature() instead.
    void addCreatureToHand(Creature *c);
};

#endif // PLAYER_H
