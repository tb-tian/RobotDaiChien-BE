## Installation

Use the package manager [pip](https://pip.pypa.io/en/stable/) to install. Highly recommend using python package manager like [conda](https://docs.conda.io/en/latest/)

Tested on python 3.12

(optional for conda user)

```bash
conda create -n CodingChallenge2025 python=3.12
conda activate CodingChallenge2025
```

install all requirements

```bash
pip install -r requirements.txt
```
## Data Folder Structure


Your projects structure should look like this

```
tree -L 2 -l .

.
├── README.md
├── requirements.txt
├── .gitignore
├── Simulator
│   ├── board.py
│   ├── FileInteractor.py
│   ├── JSONlogger.py
│   ├── listOfPlayers.py
│   ├── main.py
│   ├── Map
│   ├── Match
│   ├── player.py
│   ├── Players
│   └── powerUp.py
└── Source
    ├── bot1
    └── bot2

```

## Usage

Folder [Source](Source) is where you create your bot, including source code and executable. To use your bot, you must create a folder with your bot name inside [Players](Simulator/Players) and move your executable file there.

To run the simulator, follow this template command line
```bash
cd Simulator && python main.py map_name -p bot_1 bot_2
```

Folder [Match](Match) will include all .json file. Inside there is also a folder called [Players](Simulator/Match/Players/) that will record MAP.INP, MOVE.OUT, STATE.DAT of each turn. These files will help you understand more about your bot decision in MOVE.OUT according to MAP.INP.

Folder [Map](Map) will contain all map. We provide you a blank map for example. You can create your custom map to test your bot here

## Bug report

For bug reporting, you could report at the mail that send you this github link. Attach to it the .json file, the bot folder in [Players](Simulator/Match/Players/)