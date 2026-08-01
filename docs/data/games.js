const gamesData = [
  {
    "id": "01",
    "name": "DOOM",
    "category": "3D",
    "desc_en": "Doom-style raycasting engine.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "3D RAYCAST"
      },
      {
        "class": "game-tag tag-core",
        "text": "ACTION"
      }
    ],
    "gallery": [
      "screenshots/DOOM/D1.png",
      "screenshots/DOOM/D2.png",
      "screenshots/DOOM/D3_gif.gif",
      "screenshots/DOOM/D4_gif.gif",
      "screenshots/DOOM/D5_gif.gif"
    ],
    "desc_tr": "Doom tarzı raycasting motoru."
  },
  {
    "id": "02",
    "name": "MODE 7 RACING",
    "category": "3D",
    "desc_en": "Classic racing mechanics. Collect checkpoints and leave rivals behind.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "MODE-7"
      }
    ],
    "gallery": [
      "screenshots/Mode7/M1.png",
      "screenshots/Mode7/M2_gif.gif"
    ],
    "desc_tr": "Klasik yarış mekanikleri. Checkpointleri toplayarak rakiplerini geride bırak."
  },
  {
    "id": "03",
    "name": "WIRE-FRAME 3D",
    "category": "3D",
    "desc_en": "3D space battles against alien forces.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "POLYGON 3D"
      }
    ],
    "gallery": [
      "screenshots/Wireframe3D/W1.png",
      "screenshots/Wireframe3D/W2_gif.gif"
    ],
    "desc_tr": "Üç boyutlu uzay savaşları."
  },
  {
    "id": "04",
    "name": "GALACTIC STRIKE",
    "category": "Arcade",
    "desc_en": "Survive enemy fleets, collect power-ups and defeat bosses.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "SHOOT-EM-UP"
      }
    ],
    "gallery": [
      "screenshots/GalacticStrike/G1.png",
      "screenshots/GalacticStrike/G2_gif.gif"
    ],
    "desc_tr": "Düşman filolarına karşı hayatta kal, güçlendirmeleri topla ve bölüm sonu canavarlarını yen."
  },
  {
    "id": "05",
    "name": "PLATFORMER",
    "category": "Arcade",
    "desc_en": "Overcome obstacles, dodge traps, and beat the levels.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "PLATFORM"
      }
    ],
    "gallery": [
      "screenshots/Platformer/P1.png",
      "screenshots/Platformer/P2_gif.gif"
    ],
    "desc_tr": "Engelleri aş, tuzaklardan kaç ve bölümleri geç."
  },
  {
    "id": "06",
    "name": "SPACE INVADERS",
    "desc_en": "Survive waves of incoming aliens.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "ARCADE"
      }
    ],
    "gallery": [
      "screenshots/SpaceInvaders/S1.png",
      "screenshots/SpaceInvaders/S2_gif.gif"
    ],
    "desc_tr": "Dalga dalga gelen uzaylılara karşı hayatta kal."
  },
  {
    "id": "07",
    "name": "ARKANOID",
    "desc_en": "Break all bricks with an accelerating ball.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "ARCADE"
      }
    ],
    "gallery": [
      "screenshots/Arkanoid/A1.png",
      "screenshots/Arkanoid/A2_gif.gif"
    ],
    "desc_tr": "Giderek hızlanan topla tüm tuğlaları kır."
  },
  {
    "id": "08",
    "name": "PAC-MAN",
    "desc_en": "Escape ghosts and collect all dots in the maze.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "ARCADE"
      }
    ],
    "gallery": [
      "screenshots/Pacman/P1.png",
      "screenshots/Pacman/P2_gif.gif"
    ],
    "desc_tr": "Hayaletlerden kaç, labirentteki tüm noktaları topla."
  },
  {
    "id": "09",
    "name": "FLAPPY BIRD",
    "category": "Arcade",
    "desc_en": "Fly carefully through the pipe obstacles.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "ARCADE"
      }
    ],
    "gallery": [
      "screenshots/Flappy/F1.png",
      "screenshots/Flappy/F2.png",
      "screenshots/Flappy/F3_Gif.gif"
    ],
    "desc_tr": "Boru engellerinin arasından dikkatlice uç."
  },
  {
    "id": "10",
    "name": "SNAKE",
    "category": "Arcade",
    "desc_en": "Grow your tail, don't hit the walls or yourself.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "ARCADE"
      }
    ],
    "gallery": [
      "screenshots/Snake/S1.png",
      "screenshots/Snake/S2_gif.gif"
    ],
    "desc_tr": "Kuyruğunu uzat, duvarlara ve kendine çarpma."
  },
  {
    "id": "11",
    "name": "TETRIS",
    "category": "Puzzle",
    "desc_en": "Stack the blocks, clear the lines; speed up as the level rises.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "PUZZLE"
      },
      {
        "class": "game-tag tag-core",
        "text": "ARCADE"
      }
    ],
    "gallery": [
      "screenshots/Tetris/T1.png",
      "screenshots/Tetris/T2_gif.gif"
    ],
    "desc_tr": "Blokları yerleştir, satırları temizle; seviye yükseldikçe hızlan."
  },
  {
    "id": "12",
    "name": "DUNGEON",
    "category": "Strategy",
    "desc_en": "Top-down dungeon crawler with boss fights, a spell system, a merchant and varied biomes.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "RPG"
      },
      {
        "class": "game-tag tag-core",
        "text": "EXPLORATION"
      }
    ],
    "gallery": [
      "screenshots/Dungeon/D1.png",
      "screenshots/Dungeon/D2.png",
      "screenshots/Dungeon/D3.png",
      "screenshots/Dungeon/D4.png",
      "screenshots/Dungeon/D5_gif.gif"
    ],
    "desc_tr": "Boss savaşları, büyü sistemi, tüccar ve farklı biyomlarla top-down zindan keşfi."
  },
  {
    "id": "13",
    "name": "TOWER DEFENSE",
    "category": "Strategy",
    "desc_en": "Wave-based strategy: place towers, upgrade them and call in the waves you manage.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "STRATEGY"
      }
    ],
    "gallery": [
      "screenshots/TowerDefense/T1.png",
      "screenshots/TowerDefense/T2.png",
      "screenshots/TowerDefense/T3_gif.gif",
      "screenshots/TowerDefense/T4_gif.gif"
    ],
    "desc_tr": "Dalga tabanlı strateji: kule yerleştir, yükselt ve gelen dalgaları çağırarak yönet."
  },
  {
    "id": "14",
    "name": "2048",
    "category": "Puzzle",
    "desc_en": "Combine matching numbers to reach the 2048 tile.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "PUZZLE"
      }
    ],
    "gallery": [
      "screenshots/Game2048/2048_1.png",
      "screenshots/Game2048/2048_2_Gif.gif"
    ],
    "desc_tr": "Aynı sayıları birleştirerek 2048 karosuna ulaşmaya çalışın."
  },
  {
    "id": "15",
    "name": "RHYTHM",
    "category": "Music",
    "desc_en": "Keep up with the music beat and catch the falling neon notes.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "MUSIC"
      }
    ],
    "gallery": [
      "screenshots/Rhythm/R1.png",
      "screenshots/Rhythm/R2_gif.gif"
    ],
    "desc_tr": "Müziğin ritmine ayak uydurun ve neon notaları yakalayın."
  }
];

const appsData = [
  {
    "id": "01",
    "name": "FLIGHT TRACKER",
    "desc_en": "Near real-time flight tracking system. Track planes using the OpenSky Network API.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "APP"
      }
    ],
    "gallery": [
      "screenshots/Flight/F1.png",
      "screenshots/Flight/F2_gif.gif"
    ],
    "desc_tr": "Yakın zamanlı uçuş takip sistemi. OpenSky Network API kullanarak uçakları izleyin."
  },
  {
    "id": "02",
    "name": "TOOLS",
    "desc_en": "A basic hardware tool containing a stopwatch and a metronome.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "APP"
      }
    ],
    "gallery": [
      "screenshots/Tools/T1.png",
      "screenshots/Tools/T2.png",
      "screenshots/Tools/T3.png"
    ],
    "desc_tr": "Kronometre ve metronom içeren temel bir araç uygulaması."
  },
  {
    "id": "03",
    "name": "DRAW (ETCH-A-SKETCH)",
    "desc_en": "Classic Etch-a-Sketch style drawing app. Change modes and draw in color.",
    "tags": [
      {
        "class": "game-tag tag-core",
        "text": "APP"
      }
    ],
    "gallery": [
      "screenshots/Draw/D1.png",
      "screenshots/Draw/D2_gif.gif"
    ],
    "desc_tr": "Klasik Etch-a-Sketch tarzı çizim uygulaması. Modları değiştirerek renkli çizimler yapın."
  }
];
