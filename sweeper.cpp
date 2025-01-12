#include <iomanip>
#include <optional>
#include <ostream>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#include <sstream>

namespace
{
  struct Cell
  {
    union {
      struct {
        bool is_mine : 1;
        bool is_revealed : 1;
        bool is_flagged : 1;
        int  neighbor_mines : 4;
      };
      unsigned char value;
    } state;
  };

  enum class CoordinateSystem
  {
    Cartesian,
    Boris
  };

  struct Board
  {
    int width{20};
    int height{20};
    int num_mines{70};
    std::vector<Cell> cells;

    std::default_random_engine::result_type seed{std::random_device{}()};
    bool game_over{false};
    uint64_t time{0};
    CoordinateSystem system{CoordinateSystem::Boris};
  };

  void print_board(Board& board, bool reveal = false)
  {
    std::cout << "Seed: " << std::hex << board.seed << std::dec << '\n';
    int flagged = 0;
    std::cout << "     ";
    for( int x = 0; x < board.width; ++x )
    {
      std::cout << " " << std::setw(3) << x;
    }
    std::cout << '\n'
              << "   | ";
    for( int x = 0; x < board.width; ++x )
    {
      std::cout << "----";
    }
    std::cout << '\n';

    for (int y = 0; y < board.height; ++y)
    {
      std::cout << std::setw(3) << y << "| ";
      for( int x = 0; x < board.width; ++x )
      {
        const auto& cell = board.cells[y * board.width + x];
        if (board.game_over && cell.state.is_mine)
        {
          std::cout << " [X]";
        }
        else if (cell.state.is_flagged)
        {
          ++flagged;
          if (reveal)
          {
            std::cout << (cell.state.is_mine ? " [x]" : " [!]");
          }
          else
          {
            std::cout << " [!]";
          }
        }
        else if (reveal || cell.state.is_revealed)
        {
          if (cell.state.is_mine)
          {
            // Err, fail
            std::cout << " [X]";
          }
          else if (cell.state.neighbor_mines > 0)
          {
            std::cout << " " << std::setw(3) <<  cell.state.neighbor_mines;
          }
          else
          {
            std::cout << "    ";
          }
        }
        else
        {
          std::cout << " [ ]";
        }
      }
      std::cout << '\n';
    }
    std::cout << "   |  [" << std::setw(5) << board.num_mines - flagged << "]"
              << " [" << std::setw(5) << board.time << "]"
              << '\n';
    std::cout << std::flush;
  }

  enum CommandVerb
  {
    Reveal,
    Flag,
    New,
    Quit
  };

  struct Command
  {
    CommandVerb verb;
    int x;
    int y;
  };

  Command read_command(Board& board)
  {
    std::string line;
    while (true)
    {
      std::cout << " > " << std::flush;
      if (!std::getline(std::cin, line))
      {
        return {Quit,0,0};
      }
      std::string cmd, x, y;
      std::istringstream iss(line);
      switch (board.system)
      {
        case CoordinateSystem::Cartesian:
          iss >> cmd >> x >> y;
          break;
        case CoordinateSystem::Boris:
          iss >> cmd >> y >> x;
          break;
      }
      if (cmd == "r")
      {
        return {Reveal, std::atoi(x.c_str()), std::atoi(y.c_str())};
      }
      else if (cmd == "f" || cmd == "m")
      {
        return {Flag, std::atoi(x.c_str()), std::atoi(y.c_str())};
      }
      else if (cmd == "q")
      {
        return {Quit,0,0};
      }
      else if (cmd == "p")
      {
        print_board(board);
      }
      else if (cmd == "P")
      {
        print_board(board, true);
      }
      else if (cmd == "n")
      {
        return {New,0,0};
      }
      else if (int ix = std::atoi(cmd.c_str()); ix >= 0 && ix < board.width)
      {
        switch (board.system)
        {
          case CoordinateSystem::Cartesian:
            return {Reveal, ix, std::atoi(x.c_str())};
          case CoordinateSystem::Boris:
            return {Reveal, std::atoi(y.c_str()), ix};
        }
      }
      else
      {
        std::cout << "Unknown command. Commands: "
                  << "r x y (reveal), f x y (flag), q (quit), n [x y] (new)"
                  << std::endl;
      }
    }
  }

  template<typename F>
    requires std::invocable<F, Board&, int, int>
  void for_each_neighbour(Board& board, int x, int y, F&& f)
  {
    if (x > 0)
    {
      f(board, x - 1, y);
      if (y > 0)
      {
        f(board, x - 1, y - 1);
      }
      if (y < board.height - 1)
      {
        f(board, x - 1, y + 1);
      }
    }
    if (x < board.width - 1)
    {
      f(board, x + 1, y);
      if (y > 0)
      {
        f(board, x + 1, y - 1);
      }
      if (y < board.height - 1)
      {
        f(board, x + 1, y + 1);
      }
    }
    if (y > 0)
    {
      f(board, x, y - 1);
    }
    if (y < board.height - 1)
    {
      f(board, x, y + 1);
    }
  }


  void init_board(Board& board, int x, int y )
  {
    std::default_random_engine reng{board.seed};
    std::uniform_int_distribution<int> dist(0, board.width * board.height - 1);

    int num_mines = 0;
    int blocked = y * board.width + x;
    while (num_mines < board.num_mines)
    {
      auto pos = dist(reng);
      if (pos == blocked)
      {
        continue;
      }
      auto& cell = board.cells[pos];
      if (cell.state.is_mine)
      {
        continue;
      }
      cell.state.is_mine = true;
      cell.state.neighbor_mines = 0;
      num_mines++;
      int cx = pos % board.width;
      int cy = pos / board.width;
      for_each_neighbour(board, cx, cy,
                         [&](Board& board, int x, int y)
                         {
                           auto& cell = board.cells[y * board.width + x];
                           if (!cell.state.is_mine)
                           {
                             cell.state.neighbor_mines++;
                           }
                         });
    }
  }

  void reveal(Board& board, int x, int y)
  {
    auto& cell = board.cells[y * board.width + x];
    if (cell.state.is_revealed)
    {
      return;
    }

    cell.state.is_flagged = false;
    cell.state.is_revealed = true;

    if (cell.state.is_mine)
    {
      return;
    }

    if (cell.state.neighbor_mines > 0)
    {
      return;
    }

    for_each_neighbour(board, x, y, reveal);
  }

  void check_board(Board& board)
  {
    int correctly_flagged_mines = 0;
    int unrevealed_cells = 0;
    for( const auto&cell : board.cells )
    {
      if (cell.state.is_flagged)
      {
        if (cell.state.is_mine)
        {
          ++correctly_flagged_mines;
        }
      }
      else if (!cell.state.is_revealed)
      {
        ++unrevealed_cells;
      }
    }
    if (correctly_flagged_mines + unrevealed_cells == board.num_mines)
    {
      for( auto&cell : board.cells )
      {
        if (cell.state.is_mine)
        {
          cell.state.is_flagged = true;
        }
      }
      std::cout << "You win!" << std::endl;
      board.game_over = true;
    }
  }
}

int main(int argc, char** argv)
{
  std::span args(argv + 1, argc - 1);

  Board board;
  for (auto pos = args.begin();
       pos != args.end();
       ++pos)
  {
    std::string arg(*pos);

    if (arg.starts_with("--width="))
    {
      std::istringstream iss(arg.substr(8));
      iss >> board.width;
    }
    else if (arg.starts_with("--height="))
    {
      std::istringstream iss(arg.substr(9));
      iss >> board.height;
    }
    else if (arg.starts_with("--mines="))
    {
      std::istringstream iss(arg.substr(8));
      iss >> board.num_mines;
    }
    else if (arg.starts_with("--seed="))
    {
      std::istringstream iss(arg.substr(7));
      iss >> std::hex >> board.seed;
    }
    else if (arg == "--cartesian")
    {
      board.system = CoordinateSystem::Cartesian;
    }
  }
  board.cells.resize(board.width * board.height);

  while (true)
  {
    board.time ++;
    check_board(board);
    print_board(board);

    if (board.game_over)
    {
      std::cout << "Game over!" << std::endl;
      break;
    }

    auto command = read_command(board);
    switch(command.verb)
    {
      case New:
        {
          board.cells.clear();

          if (command.x > 0 && command.y > 0)
          {
            board.width = command.x;
            board.height = command.y;
          }
          board.cells.resize(board.width * board.height);
          board.seed = std::random_device{}();
          board.game_over = false;
          board.time = 0;
        }
        break;
      case Reveal:
        {
          if (board.time == 1)
          {
            init_board(board, command.x, command.y);
          }
          auto &cell = board.cells[command.y * board.width + command.x];
          if (cell.state.is_revealed && cell.state.neighbor_mines > 0)
          {
            // Check if all the flags are set
            int flags = 0;
            for_each_neighbour(board, command.x, command.y,
                               [&](Board& board, int x, int y)
                               {
                                 if (board.cells[y * board.width + x].state.is_flagged)
                                 {
                                   flags++;
                                 }
                               });
            if (flags != cell.state.neighbor_mines)
            {
              std::cout << "Incorrect number of flags" << std::endl;
              continue;
            }

            for_each_neighbour(board, command.x, command.y,
                               [&](Board& board, int x, int y)
                               {
                                 auto& cell = board.cells[y * board.width + x];
                                 if (!cell.state.is_mine)
                                 {
                                   reveal(board, x, y);
                                 }
                               });
          }
          else if (!cell.state.is_revealed)
          {
            reveal(board, command.x, command.y);
            if (cell.state.is_mine)
            {
              board.game_over = true;
            }
          }
        }
        break;
      case Flag:
        {
          auto& cell = board.cells[command.y * board.width + command.x];
          if (cell.state.is_revealed)
          {
            break;
          }
          cell.state.is_flagged = !cell.state.is_flagged;
        }
        break;
      case Quit:
        return 0;
    }
  }

  return 0;
}
