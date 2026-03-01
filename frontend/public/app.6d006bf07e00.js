const COLS = 11;
const ROWS = 12;

const MODE_KEYS = ['full', 'marine', 'air', 'land'];
const DIFFICULTY_KEYS = ['easy', 'medium', 'hard'];
const LANGUAGE_KEYS = ['en', 'vi'];
const PLAYER_MODE_KEYS = ['single', 'local', 'online'];
const THEME_KEYS = ['system', 'light', 'dark'];
const THEME_STORAGE_KEY = 'commander-chess-theme';
const BOARD_SKIN_KEYS = ['classic', 'warroom', 'nightops'];
const PIECE_THEME_KEYS = ['sprite', 'vector', 'minimal'];
const UI_PREF_STORAGE_KEY = 'commander-chess-ui-preferences';

const I18N = {
  en: {
    documentTitle: 'Commander Chess',
    metaDescription: 'Commander Chess is a modern strategy board game with land, air, and sea forces. Play against CPU or local multiplayer in your browser.',
    ogTitle: 'Commander Chess',
    ogDescription: 'Play Commander Chess in your browser with local CPU or local multiplayer modes.',
    setupKicker: 'Mode Setup',
    setupTitle: 'Choose How to Begin',
    setupSub: 'Quick Start runs beginner onboarding. Create Match opens full setup.',
    setupRulesOpen: 'RULES',
    setupRulesClose: 'CLOSE RULES',
    setupAboutOpen: 'About',
    languageLabel: 'Language',
    playerModeLabel: 'Player Mode',
    onlinePanelTitle: 'Online Match',
    onlinePanelHint: 'Create a room as host or join with a match code.',
    onlineMatchCodeLabel: 'Match Code',
    onlineJoinLabel: 'Join Match',
    onlineJoinPlaceholder: 'Enter match code',
    onlineCreateBtn: 'CREATE ONLINE MATCH',
    onlineJoinBtn: 'JOIN MATCH',
    onlineCopyCodeBtn: 'COPY CODE',
    onlineCopyLinkBtn: 'COPY LINK',
    onlineStatusIdle: 'No online match selected.',
    onlineStatusCreating: 'Creating online match...',
    onlineStatusCreated: 'Match {code} created. Share it and wait for your opponent.',
    onlineStatusJoining: 'Joining online match...',
    onlineStatusJoined: 'Joined match {code}.',
    onlineStatusWaitingGuest: 'Waiting for guest to join match {code}...',
    onlineStatusActive: 'Match is active. Press START GAME when both players are ready.',
    onlineStatusConnecting: 'Connecting peer link...',
    onlineStatusConnected: 'Peer link connected.',
    onlineStatusDisconnected: 'Peer link disconnected. Trying to reconnect...',
    onlineStatusNoSession: 'Create or join an online match first.',
    onlineStatusNeedGuest: 'Guest has not joined yet.',
    onlineStatusWaitingHost: 'Waiting for host to start or sync game.',
    onlineStatusMovePending: 'Move sent. Waiting for host confirmation...',
    onlineRoleHost: 'Host (RED)',
    onlineRoleGuest: 'Guest (BLUE)',
    onlineRoleUnknown: 'Unknown',
    onlineRoleLabel: 'Role: {role}',
    onlineFirestoreMissing: 'Online mode is unavailable because Firebase is not configured.',
    onlineCodeCopied: 'Match code copied.',
    onlineLinkCopied: 'Match link copied.',
    onlineClipboardFailed: 'Could not copy automatically. Please copy manually.',
    onlineInvalidMatchCode: 'Please enter a valid match code.',
    onlineMatchNotFound: 'Match not found.',
    onlineMatchFull: 'Match is already full or started.',
    onlineStartLinkHint: 'Share this link: {link}',
    rulesTitle: 'Rules of Commander Chess',
    rulesSource: 'Based on Commander Chess Rules (revised version).',
    rulesGoal: "Goal: defeat the opponent's Commander by strategy and tactics.",
    rulesBoard: 'Board: 11x12 with sea, river/reef, and land zones.',
    rulesPieces: 'Pieces: land, air, and sea forces with different roles and interactions.',
    rulesMove: 'Movement/capture: vertical and horizontal by default; some pieces leap, attack diagonally, or strike without occupying.',
    rulesCarry: 'Carrying: some pieces can transport others (piggyback) to create combined attacks.',
    rulesHero: 'Heroic piece: a piece that checkmates gains bonus movement/capture power.',
    rulesModes: 'Modes: Full Battle (capture Commander), Marine/Air/Land Battle objectives.',
    rulesSectionOverview: 'Overview & Goal',
    rulesSectionBattlefield: 'Battlefield (11x12 + terrain)',
    rulesSectionAllPieces: 'All Pieces',
    rulesSectionMovementCapture: 'Movement & Capture Rules',
    rulesSectionSpecialRules: 'Special Rules (Heroic, Transport, etc.)',
    rulesSectionWinModes: 'Win Conditions & Modes',
    rulesDetailsSummary: 'Detailed Rule Highlights',
    rulesDetail1: 'Two players: RED and BLUE. The core objective is to defeat the opposing Commander.',
    rulesDetail2: 'The board uses coordinate-style movement (vertical/horizontal axes), unlike classic square-check chess notation.',
    rulesDetail3: 'The battlefield is split into domains (sea, river/reef, land), creating different lanes and choke points.',
    rulesDetail4: 'Piece families represent modern forces: infantry, artillery, tanks, anti-air, missiles, aircraft, and warships.',
    rulesDetail5: 'Capture mechanics are asymmetric: some units capture by occupying, some bombard from range, and some can leap.',
    rulesDetail6: 'Certain attacks can be executed while holding position (stand-and-strike behavior in specific cases).',
    rulesDetail7: 'Transport/carrying is allowed for some formations, enabling combined movement and multi-layer tactics.',
    rulesDetail8: 'Heroic promotions exist: pieces that achieve decisive Commander attack gain stronger movement/capture options.',
    rulesDetail9: 'Documented variants include strategy modes (Marine/Air/Land/Raid) and full-force play.',
    rulesDetail10: 'Timed formats are described (for example 15 or 30 minutes) with score-based resolution when no objective is completed.',
    rulesDetail11: 'A strategy objective can grant bonus points; final result can depend on objective + material score.',
    rulesDetail12: 'Move annotation symbols (capture/check/checkmate/etc.) are defined in the full official rulebook.',
    rulesDetailNote: "Some tournament variants in the rulebook may differ from this web demo's mode presets.",
    rulesPiecesSummary: 'Pieces & Movement (Detailed)',
    pieceRuleC: 'C - Commander: orthogonal movement up to 10; cannot move into direct open line against the enemy Commander.',
    pieceRuleH: 'H - Headquarters: fixed base piece; normally immobile.',
    pieceRuleIn: 'In - Infantry: 1 orthogonal square on land.',
    pieceRuleM: 'M - Militia: 1 square, including diagonal on land.',
    pieceRuleT: 'T - Tank: up to 2 orthogonal on land; can fire at enemy sea targets up to 2 squares without moving to sea.',
    pieceRuleE: 'E - Engineer: 1 orthogonal on land; can ferry heavy ground fire units.',
    pieceRuleA: 'A - Artillery: up to 3 orthogonal (hero may add diagonal); river crossing is restricted unless reef lane or engineer-assisted.',
    pieceRuleAa: 'Aa - Anti-Aircraft: 1 orthogonal on land; creates anti-air defense zone around itself.',
    pieceRuleMs: 'Ms - Missile: up to 2 orthogonal movement; can strike orthogonal range 1-2 and adjacent diagonal targets (not Navy/sea).',
    pieceRuleAf: 'Af - Air Force: flying movement up to 4 in 8 directions; non-hero air can be blocked/shot by enemy anti-air coverage.',
    pieceRuleN: 'N - Navy: moves on navigable sea/river water up to 4, including diagonal; has ship/ground fire roles and can carry formations.',
    pieceRuleHero: 'Hero bonus: heroic pieces get +1 range and diagonal capability; heroic air ignores anti-air interception logic.',
    pieceRuleCarry: 'Cargo examples from engine rules: Tank/Air/Navy can carry commander-personnel; Navy supports mixed force transport combinations.',
    rulesDocOpenBtn: 'VIEW FULL RULEBOOK (DOCX)',
    rulesDocTitle: 'Commander Chess Rulebook',
    rulesDocClose: 'CLOSE',
    rulesDocHint: 'Preview is shown as HTML for browser compatibility. Download DOCX below.',
    rulesDocLink: 'DOWNLOAD DOCX',
    mainKicker: 'Tactical Command Interface',
    newGameMenu: 'NEW GAME / MENU',
    quickRestart: 'NEW GAME (SAME SETTINGS)',
    startGame: 'START GAME',
    startingGame: 'STARTING...',
    forceBotMove: 'FORCE BOT MOVE',
    flipBoard: 'FLIP BOARD',
    resetBoardView: 'RESET VIEW',
    controlDeck: 'Control Deck',
    playerSide: 'Player Side',
    difficultyField: 'Difficulty',
    themeField: 'Theme',
    settingsTitle: 'Experience',
    boardSkinLabel: 'Board Skin',
    pieceThemeLabel: 'Piece Theme',
    soundToggleLabel: 'Enable Sound',
    soundVolumeLabel: 'Sound Volume',
    animToggleLabel: 'Enable Animations',
    moveHintsLabel: 'Highlight Legal Moves',
    telemetry: 'Telemetry',
    moveHistory: 'Move History',
    historyPrevAria: 'Previous move',
    historyNextAria: 'Next move',
    historyLiveAria: 'Go to current position',
    live: 'LIVE',
    legend: 'Legend',
    about: 'About',
    aboutName: 'My name:',
    aboutPosition: 'Position:',
    aboutPositionValue: 'Vibe coder',
    aboutGithub: 'GitHub:',
    aboutEmail: 'Email:',
    authSignInGoogle: 'SIGN IN WITH GOOGLE',
    authSignOut: 'SIGN OUT',
    authGuest: 'Playing as Guest',
    authSignedInAs: 'Signed in as {name}',
    authUnavailable: 'Sign-in unavailable (Firebase Auth not enabled)',
    authConfigMissing: 'Firebase Authentication is not initialized for this project yet. Please refresh and try again.',
    authGoogleNotEnabled: 'Google sign-in is not enabled yet. Enable Google provider in Firebase Authentication.',
    authSignOutConfirm: 'Sign out now? Online match permissions may be lost.',
    legendLegal: 'Legal destination',
    legendSelected: 'Selected piece',
    legendCapture: 'Capture destination',
    legendLastMove: 'Last move target',
    legendHero: 'Heroic piece',
    riverLabel: '~ ~ ~ RIVER / SONG ~ ~ ~',
    boardAriaLabel: 'Commander chess board',
    boardRoleDescription: 'Game board',
    terrainSea: 'sea zone',
    terrainRiver: 'river zone',
    terrainLand: 'land zone',
    cellEmpty: 'empty',
    cellOccupied: '{side} {piece}',
    cellHeroFlag: 'heroic',
    cellSelectedFlag: 'selected',
    cellLegalMoveFlag: 'legal move',
    cellLegalCaptureFlag: 'legal capture',
    cellLastToFlag: 'last move destination',
    cellLastFromFlag: 'last move origin',
    cellLabel: 'Column {col}, row {row}, {terrain}, {occupancy}{flags}',
    pieceNameByCode: {
      C: 'Commander',
      H: 'Headquarters',
      In: 'Infantry',
      M: 'Militia',
      T: 'Tank',
      E: 'Engineer',
      A: 'Artillery',
      Aa: 'Anti-Aircraft',
      Ms: 'Missile',
      Af: 'Air Force',
      N: 'Navy'
    },
    statYou: 'You',
    statDifficulty: 'Difficulty',
    statTurn: 'Turn',
    statLegal: 'Legal Moves',
    statPieces: 'Active Pieces',
    statSelection: 'Selection',
    noSelection: 'None',
    bothSides: 'BOTH',
    noMovesYet: 'No moves yet',
    statusIdle: 'IDLE',
    statusReview: 'REVIEW',
    statusGameOver: 'GAME OVER',
    statusThinkingTag: 'THINKING',
    statusToMoveTag: '{side} TO MOVE',
    statusPressStart: 'Press START GAME',
    newGameConfirmPrompt: 'Start a new game? Current game will be lost.',
    selectSideBegin: 'Select your side and begin.',
    reviewingInitial: 'Reviewing initial setup',
    reviewingMove: 'Reviewing move {index}/{total}',
    gameOver: 'Game Over: {result}',
    cpuThinking: 'CPU thinking ({side})...',
    yourTurn: 'Your turn ({side})',
    localTurn: 'Turn: {side}',
    onlineTurn: 'Online turn ({side})',
    metaNoGame: 'Mode: {mode} | Player Mode: {playerMode} | Difficulty: {difficulty} | Side: {side}',
    lastMoveNone: 'none',
    captureSuffix: ' capture',
    reviewMeta: ' | REVIEW MODE (LIVE to return)',
    metaWithGame: 'Mode: {mode} | Player Mode: {playerMode} | Difficulty: {difficulty} | Game: {game} | Last move: {lastMove}{reviewMeta}',
    errorPrefix: 'Error: {msg}',
    requestFailed: 'Request failed. Please try again.',
    engineUnavailable: 'Cannot connect to game engine. Please check your connection and try again.',
    retryAction: 'RETRY',
    retryHint: 'Press RETRY to try again.',
    historyFormat: '{index}. {player} {from} -> {to}{capture}',
    selectionFormat: '{player} {kind}{hero} @ ({col},{row})',
    modeLabel: {
      full: 'Full Battle',
      marine: 'Marine Battle',
      air: 'Air Battle',
      land: 'Land Battle'
    },
    modeDesc: {
      full: 'Capture the enemy Commander.',
      marine: 'Destroy 2 enemy Navies.',
      air: 'Destroy 2 enemy Air Forces.',
      land: 'Destroy core land units.'
    },
    difficultyLabel: {
      easy: 'Beginner',
      medium: 'Intermediate',
      hard: 'Expert'
    },
    difficultyDesc: {
      easy: 'Depth 4, no MCTS.',
      medium: 'Depth 6, no MCTS.',
      hard: 'Depth 8 with MCTS.'
    },
    languageLabelByCode: {
      en: 'English',
      vi: 'Vietnamese'
    },
    playerModeLabelByCode: {
      single: 'Single Player (vs CPU)',
      local: 'Local Multiplayer (2 players)',
      online: 'Online Multiplayer (P2P)'
    },
    playerModeShortLabelByCode: {
      single: 'Single',
      local: 'Local',
      online: 'Online'
    },
    sideSetupLabel: {
      red: 'PLAY AS RED',
      blue: 'PLAY AS BLUE'
    },
    sideSelectLabel: {
      red: 'Play Red',
      blue: 'Play Blue'
    },
    themeLabelByCode: {
      system: 'System',
      light: 'Light',
      dark: 'Dark'
    },
    boardSkinLabelByCode: {
      classic: 'Classic Grid',
      warroom: 'War Room',
      nightops: 'Night Ops'
    },
    pieceThemeLabelByCode: {
      sprite: 'Illustrated',
      vector: 'Vector Badges',
      minimal: 'Minimal Glyphs'
    },
    sideCaps: {
      red: 'RED',
      blue: 'BLUE'
    }
  },
  vi: {
    documentTitle: 'Cờ Tư Lệnh',
    metaDescription: 'Cờ Tư Lệnh là trò chơi bàn cờ chiến thuật hiện đại với lực lượng bộ, không, hải. Chơi với CPU hoặc nhiều người cục bộ ngay trên trình duyệt.',
    ogTitle: 'Cờ Tư Lệnh',
    ogDescription: 'Chơi Cờ Tư Lệnh trên trình duyệt với CPU cục bộ hoặc nhiều người chơi cục bộ.',
    setupKicker: 'Thiết lập trận',
    setupTitle: 'Chọn Cách Bắt Đầu',
    setupSub: 'Vào nhanh sẽ chạy luật + hướng dẫn. Tạo trận sẽ mở thiết lập đầy đủ.',
    setupRulesOpen: 'LUẬT',
    setupRulesClose: 'ĐÓNG LUẬT',
    setupAboutOpen: 'Giới thiệu',
    languageLabel: 'Ngôn ngữ',
    playerModeLabel: 'Chế độ chơi',
    onlinePanelTitle: 'Trận đấu trực tuyến',
    onlinePanelHint: 'Tạo phòng với vai trò chủ phòng hoặc tham gia bằng mã trận.',
    onlineMatchCodeLabel: 'Mã trận',
    onlineJoinLabel: 'Tham gia trận',
    onlineJoinPlaceholder: 'Nhập mã trận',
    onlineCreateBtn: 'TẠO TRẬN TRỰC TUYẾN',
    onlineJoinBtn: 'THAM GIA',
    onlineCopyCodeBtn: 'SAO CHÉP MÃ',
    onlineCopyLinkBtn: 'SAO CHÉP LINK',
    onlineStatusIdle: 'Chưa chọn trận trực tuyến.',
    onlineStatusCreating: 'Đang tạo trận trực tuyến...',
    onlineStatusCreated: 'Đã tạo trận {code}. Hãy chia sẻ mã và chờ đối thủ.',
    onlineStatusJoining: 'Đang tham gia trận...',
    onlineStatusJoined: 'Đã tham gia trận {code}.',
    onlineStatusWaitingGuest: 'Đang chờ người chơi thứ hai vào trận {code}...',
    onlineStatusActive: 'Trận đã sẵn sàng. Nhấn BẮT ĐẦU khi cả hai đã chuẩn bị.',
    onlineStatusConnecting: 'Đang kết nối ngang hàng...',
    onlineStatusConnected: 'Đã kết nối ngang hàng.',
    onlineStatusDisconnected: 'Mất kết nối ngang hàng. Đang thử kết nối lại...',
    onlineStatusNoSession: 'Hãy tạo hoặc tham gia một trận trực tuyến trước.',
    onlineStatusNeedGuest: 'Chưa có người chơi thứ hai tham gia.',
    onlineStatusWaitingHost: 'Đang chờ chủ phòng bắt đầu hoặc đồng bộ ván.',
    onlineStatusMovePending: 'Đã gửi nước đi. Đang chờ chủ phòng xác nhận...',
    onlineRoleHost: 'Chủ phòng (ĐỎ)',
    onlineRoleGuest: 'Khách (XANH)',
    onlineRoleUnknown: 'Chưa rõ',
    onlineRoleLabel: 'Vai trò: {role}',
    onlineFirestoreMissing: 'Không thể dùng chế độ trực tuyến vì Firebase chưa được cấu hình.',
    onlineCodeCopied: 'Đã sao chép mã trận.',
    onlineLinkCopied: 'Đã sao chép link trận.',
    onlineClipboardFailed: 'Không thể tự động sao chép. Vui lòng sao chép thủ công.',
    onlineInvalidMatchCode: 'Vui lòng nhập mã trận hợp lệ.',
    onlineMatchNotFound: 'Không tìm thấy trận.',
    onlineMatchFull: 'Trận đã đầy hoặc đã bắt đầu.',
    onlineStartLinkHint: 'Chia sẻ link này: {link}',
    rulesTitle: 'Luật Cờ Tư Lệnh',
    rulesSource: 'Tóm tắt theo tài liệu Commander Chess Rules (bản chỉnh sửa).',
    rulesGoal: 'Mục tiêu: dùng chiến lược và chiến thuật để hạ Tư lệnh đối phương.',
    rulesBoard: 'Bàn cờ: 11x12 gồm vùng biển, sông/rạn và đất liền.',
    rulesPieces: 'Quân cờ: lực lượng bộ, không, hải với vai trò và tương tác khác nhau.',
    rulesMove: 'Di chuyển/bắt: mặc định theo trục dọc-ngang; một số quân có thể nhảy, bắt chéo hoặc bắn từ vị trí đứng yên.',
    rulesCarry: 'Chở quân: một số quân có thể chở quân khác (cõng quân) để phối hợp tấn công.',
    rulesHero: 'Quân anh hùng: quân lập công chiếu hết được tăng sức di chuyển và bắt.',
    rulesModes: 'Chế độ: Toàn chiến (bắt Tư lệnh), hoặc mục tiêu Hải chiến/Không chiến/Lục chiến.',
    rulesSectionOverview: 'Tổng quan & Mục tiêu',
    rulesSectionBattlefield: 'Chiến trường (11x12 + địa hình)',
    rulesSectionAllPieces: 'Toàn bộ quân cờ',
    rulesSectionMovementCapture: 'Luật di chuyển & bắt quân',
    rulesSectionSpecialRules: 'Luật đặc biệt (Anh hùng, Chở quân, ...)',
    rulesSectionWinModes: 'Điều kiện thắng & Chế độ',
    rulesDetailsSummary: 'Chi tiết luật chơi',
    rulesDetail1: 'Hai bên chơi: ĐỎ và XANH. Mục tiêu cốt lõi là đánh bại Tư lệnh đối phương.',
    rulesDetail2: 'Bàn cờ dùng hệ trục tọa độ khi di chuyển (dọc/ngang), khác kiểu ô vuông của cờ vua.',
    rulesDetail3: 'Chiến trường chia theo miền (biển, sông/rạn, đất liền), tạo các hướng tiến công và điểm nghẽn.',
    rulesDetail4: 'Hệ quân mô phỏng lực lượng hiện đại: bộ binh, pháo, xe tăng, phòng không, tên lửa, không quân, hải quân.',
    rulesDetail5: 'Cơ chế bắt quân bất đối xứng: có quân bắt bằng chiếm ô, có quân bắn tầm xa, có quân nhảy qua quân khác.',
    rulesDetail6: 'Một số tình huống cho phép tấn công khi đứng yên (đánh tại chỗ) theo đúng loại quân.',
    rulesDetail7: 'Có cơ chế chở/cõng quân trong một số đội hình, giúp tạo đòn phối hợp nhiều tầng.',
    rulesDetail8: 'Có phong quân anh hùng: quân lập công quyết định trước Tư lệnh sẽ được tăng sức đi và bắt.',
    rulesDetail9: 'Tài liệu có nhiều biến thể: Hải chiến/Không chiến/Lục chiến/Đột kích và toàn lực.',
    rulesDetail10: 'Có thể chơi theo thời gian (ví dụ 15 hoặc 30 phút), nếu chưa đạt mục tiêu thì tính điểm để phân thắng bại.',
    rulesDetail11: 'Mục tiêu chiến lược có thể cộng điểm thưởng; kết quả cuối cùng có thể phụ thuộc mục tiêu + điểm vật chất.',
    rulesDetail12: 'Ký hiệu biên bản nước đi (bắt/chiếu/chiếu hết...) được nêu trong tài liệu luật đầy đủ.',
    rulesDetailNote: 'Một số thể thức thi đấu trong tài liệu có thể khác với preset của bản web demo.',
    rulesPiecesSummary: 'Quân cờ & Cách đi (Chi tiết)',
    pieceRuleC: 'C - Tư lệnh: đi dọc/ngang tối đa 10 ô; không được đứng vào thế nhìn thẳng trực diện với Tư lệnh đối phương.',
    pieceRuleH: 'H - Sở chỉ huy: quân căn cứ, thông thường không di chuyển.',
    pieceRuleIn: 'In - Bộ binh: đi 1 ô theo trục dọc/ngang trên đất liền.',
    pieceRuleM: 'M - Dân quân: đi 1 ô, gồm cả đường chéo trên đất liền.',
    pieceRuleT: 'T - Xe tăng: đi tối đa 2 ô dọc/ngang trên đất liền; có thể bắn mục tiêu trên biển trong tầm 2 ô mà không xuống biển.',
    pieceRuleE: 'E - Công binh: đi 1 ô dọc/ngang trên đất liền; có thể chở nhóm hỏa lực mặt đất hạng nặng.',
    pieceRuleA: 'A - Pháo binh: đi tối đa 3 ô dọc/ngang (anh hùng có thể thêm đường chéo); qua sông bị hạn chế nếu không ở làn rạn hoặc không được công binh chở.',
    pieceRuleAa: 'Aa - Phòng không: đi 1 ô dọc/ngang trên đất liền; tạo vùng khống chế phòng không quanh nó.',
    pieceRuleMs: 'Ms - Tên lửa: đi tối đa 2 ô dọc/ngang; có thể đánh mục tiêu theo hướng dọc/ngang tầm 1-2 và chéo gần (không đánh Hải quân/ô biển).',
    pieceRuleAf: 'Af - Không quân: bay tối đa 4 ô theo 8 hướng; không quân chưa anh hùng có thể bị chặn/bắn bởi vùng phòng không địch.',
    pieceRuleN: 'N - Hải quân: di chuyển trên vùng nước biển/sông hợp lệ tối đa 4 ô, gồm đường chéo; có vai trò hỏa lực hải-địa và chở đội hình.',
    pieceRuleHero: 'Thưởng anh hùng: quân anh hùng được +1 tầm đi và có quyền đi chéo; không quân anh hùng bỏ qua cơ chế chặn/bắn của phòng không.',
    pieceRuleCarry: 'Ví dụ chở quân theo engine: Xe tăng/Không quân/Hải quân có thể chở nhóm người gồm cả Tư lệnh; Hải quân có các tổ hợp chở lực lượng hỗn hợp.',
    rulesDocOpenBtn: 'XEM TOÀN BỘ LUẬT (DOCX)',
    rulesDocTitle: 'Tài liệu Luật Cờ Tư Lệnh',
    rulesDocClose: 'ĐÓNG',
    rulesDocHint: 'Bản xem trong ứng dụng dùng HTML để tương thích trình duyệt. Bạn có thể tải DOCX bên dưới.',
    rulesDocLink: 'TẢI DOCX',
    mainKicker: 'Giao diện chỉ huy chiến thuật',
    newGameMenu: 'VÁN MỚI / MENU',
    quickRestart: 'VÁN MỚI (GIỮ THIẾT LẬP)',
    startGame: 'BẮT ĐẦU',
    startingGame: 'ĐANG BẮT ĐẦU...',
    forceBotMove: 'ÉP BOT ĐI',
    flipBoard: 'LẬT BÀN CỜ',
    resetBoardView: 'ĐẶT LẠI GÓC NHÌN',
    controlDeck: 'Bảng điều khiển',
    playerSide: 'Phe người chơi',
    difficultyField: 'Độ khó',
    themeField: 'Giao diện',
    settingsTitle: 'Trải nghiệm',
    boardSkinLabel: 'Mặt bàn cờ',
    pieceThemeLabel: 'Kiểu quân cờ',
    soundToggleLabel: 'Bật âm thanh',
    soundVolumeLabel: 'Âm lượng',
    animToggleLabel: 'Bật hiệu ứng động',
    moveHintsLabel: 'Tô sáng nước đi hợp lệ',
    telemetry: 'Thông số',
    moveHistory: 'Lịch sử nước đi',
    historyPrevAria: 'Nước trước',
    historyNextAria: 'Nước tiếp theo',
    historyLiveAria: 'Về thế cờ hiện tại',
    live: 'HIỆN TẠI',
    legend: 'Chú thích',
    about: 'Giới thiệu',
    aboutName: 'Tên tôi:',
    aboutPosition: 'Vai trò:',
    aboutPositionValue: 'Vibe coder',
    aboutGithub: 'GitHub:',
    aboutEmail: 'Email:',
    authSignInGoogle: 'ĐĂNG NHẬP GOOGLE',
    authSignOut: 'ĐĂNG XUẤT',
    authGuest: 'Đang chơi với tư cách Khách',
    authSignedInAs: 'Đã đăng nhập: {name}',
    authUnavailable: 'Không thể đăng nhập (Firebase Auth chưa bật)',
    authConfigMissing: 'Firebase Authentication chưa được khởi tạo cho dự án này. Vui lòng tải lại và thử lại.',
    authGoogleNotEnabled: 'Google đăng nhập chưa được bật. Hãy bật nhà cung cấp Google trong Firebase Authentication.',
    authSignOutConfirm: 'Đăng xuất ngay? Trận trực tuyến có thể mất quyền cập nhật.',
    legendLegal: 'Ô có thể đi',
    legendSelected: 'Quân đang chọn',
    legendCapture: 'Ô bắt quân',
    legendLastMove: 'Ô nước đi cuối',
    legendHero: 'Quân anh hùng',
    riverLabel: '~ ~ ~ SÔNG ~ ~ ~',
    boardAriaLabel: 'Bàn cờ Commander Chess',
    boardRoleDescription: 'Bàn cờ trận đấu',
    terrainSea: 'vùng biển',
    terrainRiver: 'vùng sông',
    terrainLand: 'vùng đất liền',
    cellEmpty: 'trống',
    cellOccupied: '{side} {piece}',
    cellHeroFlag: 'quân anh hùng',
    cellSelectedFlag: 'đang chọn',
    cellLegalMoveFlag: 'nước đi hợp lệ',
    cellLegalCaptureFlag: 'nước bắt hợp lệ',
    cellLastToFlag: 'đích của nước đi cuối',
    cellLastFromFlag: 'điểm xuất phát của nước đi cuối',
    cellLabel: 'Cột {col}, hàng {row}, {terrain}, {occupancy}{flags}',
    pieceNameByCode: {
      C: 'Tư lệnh',
      H: 'Sở chỉ huy',
      In: 'Bộ binh',
      M: 'Dân quân',
      T: 'Xe tăng',
      E: 'Công binh',
      A: 'Pháo binh',
      Aa: 'Phòng không',
      Ms: 'Tên lửa',
      Af: 'Không quân',
      N: 'Hải quân'
    },
    statYou: 'Bạn',
    statDifficulty: 'Độ khó',
    statTurn: 'Lượt',
    statLegal: 'Nước đi hợp lệ',
    statPieces: 'Quân đang hoạt động',
    statSelection: 'Đang chọn',
    noSelection: 'Không',
    bothSides: 'CẢ HAI',
    noMovesYet: 'Chưa có nước đi',
    statusIdle: 'CHỜ',
    statusReview: 'XEM LẠI',
    statusGameOver: 'KẾT THÚC',
    statusThinkingTag: 'ĐANG TÍNH',
    statusToMoveTag: '{side} ĐI',
    statusPressStart: 'Nhấn BẮT ĐẦU',
    newGameConfirmPrompt: 'Bắt đầu ván mới? Ván hiện tại sẽ bị mất.',
    selectSideBegin: 'Chọn phe của bạn rồi bắt đầu.',
    reviewingInitial: 'Đang xem thế cờ ban đầu',
    reviewingMove: 'Đang xem nước {index}/{total}',
    gameOver: 'Ván kết thúc: {result}',
    cpuThinking: 'CPU đang tính ({side})...',
    yourTurn: 'Lượt của bạn ({side})',
    localTurn: 'Lượt: {side}',
    onlineTurn: 'Lượt trực tuyến ({side})',
    metaNoGame: 'Chế độ: {mode} | Chế độ chơi: {playerMode} | Độ khó: {difficulty} | Phe: {side}',
    lastMoveNone: 'không có',
    captureSuffix: ' bắt',
    reviewMeta: ' | CHẾ ĐỘ XEM LẠI (LIVE để quay lại)',
    metaWithGame: 'Chế độ: {mode} | Chế độ chơi: {playerMode} | Độ khó: {difficulty} | Ván: {game} | Nước cuối: {lastMove}{reviewMeta}',
    errorPrefix: 'Lỗi: {msg}',
    requestFailed: 'Yêu cầu thất bại. Vui lòng thử lại.',
    engineUnavailable: 'Không thể kết nối tới engine trò chơi. Vui lòng kiểm tra kết nối và thử lại.',
    retryAction: 'THỬ LẠI',
    retryHint: 'Nhấn THỬ LẠI để thử lại.',
    historyFormat: '{index}. {player} {from} -> {to}{capture}',
    selectionFormat: '{player} {kind}{hero} @ ({col},{row})',
    modeLabel: {
      full: 'Toàn chiến',
      marine: 'Hải chiến',
      air: 'Không chiến',
      land: 'Lục chiến'
    },
    modeDesc: {
      full: 'Bắt Tư lệnh đối phương.',
      marine: 'Phá hủy 2 Hải quân đối phương.',
      air: 'Phá hủy 2 Không quân đối phương.',
      land: 'Phá hủy các đơn vị bộ binh chủ lực.'
    },
    difficultyLabel: {
      easy: 'Dễ',
      medium: 'Trung bình',
      hard: 'Khó'
    },
    difficultyDesc: {
      easy: 'Độ sâu 4, không MCTS.',
      medium: 'Độ sâu 6, không MCTS.',
      hard: 'Độ sâu 8, có MCTS.'
    },
    languageLabelByCode: {
      en: 'English',
      vi: 'Tiếng Việt'
    },
    playerModeLabelByCode: {
      single: 'Một người chơi (đấu CPU)',
      local: 'Nhiều người cục bộ (2 người)',
      online: 'Nhiều người trực tuyến (P2P)'
    },
    playerModeShortLabelByCode: {
      single: 'Một người',
      local: 'Cục bộ',
      online: 'Trực tuyến'
    },
    sideSetupLabel: {
      red: 'CHƠI BÊN ĐỎ',
      blue: 'CHƠI BÊN XANH'
    },
    sideSelectLabel: {
      red: 'Chơi bên Đỏ',
      blue: 'Chơi bên Xanh'
    },
    themeLabelByCode: {
      system: 'Theo hệ thống',
      light: 'Sáng',
      dark: 'Tối'
    },
    boardSkinLabelByCode: {
      classic: 'Lưới cổ điển',
      warroom: 'Phòng tác chiến',
      nightops: 'Tác chiến đêm'
    },
    pieceThemeLabelByCode: {
      sprite: 'Minh họa',
      vector: 'Huy hiệu vector',
      minimal: 'Biểu tượng tối giản'
    },
    sideCaps: {
      red: 'ĐỎ',
      blue: 'XANH'
    }
  }
};

const AUDIO_SAMPLE_RATE = 22050;
const SOUND_LIBRARY = {
  radio: {
    notes: [
      { freq: 1400, dur: 0.015, decay: 85 },
      { freq: 980, dur: 0.02, decay: 70 }
    ],
    volume: 0.20
  },
  move: {
    notes: [
      { freq: 600, dur: 0.05, decay: 25 },
      { freq: 400, dur: 0.07, decay: 30 }
    ],
    volume: 0.30
  },
  rumble: {
    notes: [
      { freq: 90, dur: 0.06, decay: 12 },
      { freq: 70, dur: 0.07, decay: 10 },
      { freq: 58, dur: 0.08, decay: 9 }
    ],
    volume: 0.36
  },
  capture: {
    notes: [
      { freq: 220, dur: 0.06, decay: 20 },
      { freq: 160, dur: 0.10, decay: 15 },
      { freq: 100, dur: 0.08, decay: 18 }
    ],
    volume: 0.50
  },
  hero: {
    notes: [
      { freq: 523, dur: 0.10, decay: 8 },
      { freq: 784, dur: 0.18, decay: 6 }
    ],
    volume: 0.45
  },
  win: {
    notes: [
      { freq: 523, dur: 0.12, decay: 4 },
      { freq: 659, dur: 0.12, decay: 4 },
      { freq: 784, dur: 0.12, decay: 4 },
      { freq: 1047, dur: 0.28, decay: 3 }
    ],
    volume: 0.50
  },
  horn: {
    notes: [
      { freq: 220, dur: 0.11, decay: 5 },
      { freq: 330, dur: 0.13, decay: 5 },
      { freq: 440, dur: 0.18, decay: 4 },
      { freq: 554, dur: 0.26, decay: 3 }
    ],
    volume: 0.52
  },
  invalid: {
    notes: [
      { freq: 180, dur: 0.05, decay: 30 },
      { freq: 120, dur: 0.08, decay: 25 }
    ],
    volume: 0.30
  },
  cpu: {
    notes: [{ freq: 800, dur: 0.04, decay: 40 }],
    volume: 0.18
  },
  boom: {
    notes: [
      { freq: 120, dur: 0.04, decay: 8 },
      { freq: 80, dur: 0.08, decay: 5 },
      { freq: 50, dur: 0.12, decay: 4 }
    ],
    volume: 0.55
  }
};

const DEFAULT_UI_PREFS = Object.freeze({
  boardSkin: 'classic',
  pieceTheme: 'sprite',
  soundEnabled: true,
  soundVolume: 0.9,
  animationsEnabled: true,
  moveHints: true
});

const PIECE_GLYPH = Object.freeze({
  C: 'CMD',
  H: 'HQ',
  In: 'IN',
  M: 'MI',
  T: 'TK',
  E: 'EN',
  A: 'AR',
  Aa: 'AA',
  Ms: 'MS',
  Af: 'AF',
  N: 'NV'
});

const TUTORIAL_STORAGE_KEY = 'tutorialCompleted';
const QUICK_TUTORIAL_STORAGE_KEY = 'quickTutorialPromptSeen';
const REPLAY_MAX_MOVES = 2048;

const TUTORIAL_STEPS = Object.freeze([
  {
    id: 'welcome',
    title: 'Welcome to Commander Chess',
    text: 'This guided mode covers all pieces. You can move each piece freely and take unlimited turns before continuing.',
    mode: 'welcome'
  },
  {
    id: 'terrain',
    title: 'Board Terrain',
    text: 'Sea, river, and land zones are highlighted with pulsing borders. Terrain affects movement options.',
    mode: 'terrain'
  },
  {
    id: 'commander',
    title: 'Commander Movement',
    text: 'Drag the Commander freely to any green legal square. Try multiple moves before continuing.',
    mode: 'move',
    pieceId: 'commander'
  },
  {
    id: 'headquarters',
    title: 'Headquarters',
    text: 'Headquarters is a fixed base unit in this ruleset. It has no standard movement.',
    mode: 'move',
    pieceId: 'headquarters'
  },
  {
    id: 'infantry',
    title: 'Infantry Movement',
    text: 'Infantry moves 1 square orthogonally on land. Move it as many times as you like.',
    mode: 'move',
    pieceId: 'infantry'
  },
  {
    id: 'militia',
    title: 'Militia Movement',
    text: 'Militia moves 1 square in any direction on land, including diagonals.',
    mode: 'move',
    pieceId: 'militia'
  },
  {
    id: 'tank',
    title: 'Tank Movement',
    text: 'Tank moves up to 2 orthogonal land squares. Try short and long tank moves.',
    mode: 'move',
    pieceId: 'tank'
  },
  {
    id: 'engineer',
    title: 'Engineer Movement',
    text: 'Engineer moves 1 orthogonal square on land and enables transport tactics in real games.',
    mode: 'move',
    pieceId: 'engineer'
  },
  {
    id: 'artillery',
    title: 'Artillery Movement',
    text: 'Artillery moves up to 3 orthogonal land squares (heroic versions gain more options).',
    mode: 'move',
    pieceId: 'artillery'
  },
  {
    id: 'antiair',
    title: 'Anti-Aircraft Movement',
    text: 'Anti-Aircraft moves 1 orthogonal land square and creates anti-air pressure zones.',
    mode: 'move',
    pieceId: 'antiair'
  },
  {
    id: 'missile',
    title: 'Missile Movement',
    text: 'Missile moves up to 2 orthogonal squares. Practice repositioning for strike lines.',
    mode: 'move',
    pieceId: 'missile'
  },
  {
    id: 'plane',
    title: 'Air Force Movement',
    text: 'Air Force moves up to 4 squares in 8 directions. Move it freely to feel aerial mobility.',
    mode: 'move',
    pieceId: 'plane'
  },
  {
    id: 'transport',
    title: 'Transport Unit',
    text: 'Navy moves on sea/river water up to 4 squares in 8 directions. Use unlimited practice turns.',
    mode: 'move',
    pieceId: 'transport'
  },
  {
    id: 'hero',
    title: 'Heroic Upgrade',
    text: 'Heroic pieces gain +1 range and diagonal capability. Move this Heroic Tank as much as you want.',
    mode: 'move',
    pieceId: 'hero'
  }
]);

const TUTORIAL_INITIAL_PIECES = Object.freeze([
  { id: 'commander', kind: 'C', player: 'red', hero: false, col: 5, row: 2 },
  { id: 'headquarters', kind: 'H', player: 'red', hero: false, col: 6, row: 2 },
  { id: 'infantry', kind: 'In', player: 'red', hero: false, col: 5, row: 4 },
  { id: 'militia', kind: 'M', player: 'red', hero: false, col: 6, row: 4 },
  { id: 'tank', kind: 'T', player: 'red', hero: false, col: 4, row: 4 },
  { id: 'engineer', kind: 'E', player: 'red', hero: false, col: 7, row: 4 },
  { id: 'artillery', kind: 'A', player: 'red', hero: false, col: 5, row: 7 },
  { id: 'antiair', kind: 'Aa', player: 'red', hero: false, col: 6, row: 7 },
  { id: 'missile', kind: 'Ms', player: 'red', hero: false, col: 7, row: 7 },
  { id: 'plane', kind: 'Af', player: 'red', hero: false, col: 8, row: 3 },
  { id: 'transport', kind: 'N', player: 'red', hero: false, col: 1, row: 6 },
  { id: 'hero', kind: 'T', player: 'red', hero: true, col: 5, row: 9 }
]);

const boardEl = document.getElementById('board');
const boardStageEl = document.querySelector('.board-stage');
const boardFrameEl = document.querySelector('.board-frame');
const statusBarEl = document.getElementById('statusBar');
const statusEl = document.getElementById('status');
const statusTagEl = document.getElementById('statusTag');
const metaEl = document.getElementById('meta');
const statusLiveEl = document.getElementById('statusLive');
const retryBtn = document.getElementById('retryBtn');
const metaDescriptionTagEl = document.querySelector('meta[name="description"]');
const ogTitleTagEl = document.querySelector('meta[property="og:title"]');
const ogDescriptionTagEl = document.querySelector('meta[property="og:description"]');
const ogUrlTagEl = document.querySelector('meta[property="og:url"]');

const yCoordsEl = document.getElementById('yCoords');
const xCoordsEl = document.getElementById('xCoords');

const newBtn = document.getElementById('newBtn');
const quickRestartBtn = document.getElementById('quickRestartBtn');
const botBtn = document.getElementById('botBtn');
const flipBtn = document.getElementById('flipBtn');
const sideSelect = document.getElementById('sideSelect');
const difficultySelect = document.getElementById('difficultySelect');
const themeSelect = document.getElementById('themeSelect');
const boardSkinSelect = document.getElementById('boardSkinSelect');
const pieceThemeSelect = document.getElementById('pieceThemeSelect');
const soundToggleEl = document.getElementById('soundToggle');
const soundVolumeRangeEl = document.getElementById('soundVolumeRange');
const animToggleEl = document.getElementById('animToggle');
const moveHintsToggleEl = document.getElementById('moveHintsToggle');

const statYouEl = document.getElementById('statYou');
const statDifficultyEl = document.getElementById('statDifficulty');
const statTurnEl = document.getElementById('statTurn');
const statLegalEl = document.getElementById('statLegal');
const statPiecesEl = document.getElementById('statPieces');
const statSelectionEl = document.getElementById('statSelection');
const histPrevBtn = document.getElementById('histPrevBtn');
const histLiveBtn = document.getElementById('histLiveBtn');
const histNextBtn = document.getElementById('histNextBtn');
const historyListEl = document.getElementById('historyList');

const setupOverlayEl = document.getElementById('setupOverlay');
const setupKickerEl = document.getElementById('setupKicker');
const setupTitleEl = document.getElementById('setupTitle');
const setupSubEl = document.getElementById('setupSub');
const presetQuickStartBtn = document.getElementById('presetQuickStartBtn');
const presetClassicBtn = document.getElementById('presetClassicBtn');
const presetCustomBtn = document.getElementById('presetCustomBtn');
const setupAboutToggleBtn = document.getElementById('setupAboutToggleBtn');
const setupAboutModalEl = document.getElementById('setupAboutModal');
const setupAboutCardEl = setupAboutModalEl ? setupAboutModalEl.querySelector('.setup-about-card') : null;
const setupAboutCloseBtn = document.getElementById('setupAboutCloseBtn');
const setupAboutTitleEl = document.getElementById('setupAboutTitle');
const setupAboutNameLabelEl = document.getElementById('setupAboutNameLabel');
const setupAboutPositionLabelEl = document.getElementById('setupAboutPositionLabel');
const setupAboutPositionValueEl = document.getElementById('setupAboutPositionValue');
const setupAboutGithubLabelEl = document.getElementById('setupAboutGithubLabel');
const setupDetailOptionsEl = document.getElementById('setupDetailOptions');
const setupRulesToggleBtn = document.getElementById('setupRulesToggleBtn');
const languageLabelEl = document.getElementById('languageLabel');
const languageButtonsEl = document.getElementById('languageButtons');
const setupThemeLabelEl = document.getElementById('setupThemeLabel');
const setupThemeButtonsEl = document.getElementById('setupThemeButtons');
const playerModeLabelEl = document.getElementById('playerModeLabel');
const playerModeButtonsEl = document.getElementById('playerModeButtons');
const onlinePanelEl = document.getElementById('onlinePanel');
const onlinePanelTitleEl = document.getElementById('onlinePanelTitle');
const onlinePanelHintEl = document.getElementById('onlinePanelHint');
const onlineMatchCodeLabelEl = document.getElementById('onlineMatchCodeLabel');
const onlineMatchCodeEl = document.getElementById('onlineMatchCode');
const copyOnlineCodeBtn = document.getElementById('copyOnlineCodeBtn');
const copyOnlineLinkBtn = document.getElementById('copyOnlineLinkBtn');
const joinMatchInputEl = document.getElementById('joinMatchInput');
const joinMatchInputLabelEl = document.getElementById('joinMatchInputLabel');
const createOnlineBtn = document.getElementById('createOnlineBtn');
const joinOnlineBtn = document.getElementById('joinOnlineBtn');
const onlineStatusTextEl = document.getElementById('onlineStatusText');
const modeButtonsEl = document.getElementById('modeButtons');
const sideButtonsEl = document.getElementById('sideButtons');
const difficultyButtonsEl = document.getElementById('difficultyButtons');
const rulesTitleEl = document.getElementById('rulesTitle');
const rulesSourceEl = document.getElementById('rulesSource');
const rulesAccordionEl = document.getElementById('rulesAccordion');
const rulesSectionOverviewEl = document.getElementById('rulesSectionOverview');
const rulesSectionBattlefieldEl = document.getElementById('rulesSectionBattlefield');
const rulesSectionAllPiecesEl = document.getElementById('rulesSectionAllPieces');
const rulesSectionMovementCaptureEl = document.getElementById('rulesSectionMovementCapture');
const rulesSectionSpecialRulesEl = document.getElementById('rulesSectionSpecialRules');
const rulesSectionWinModesEl = document.getElementById('rulesSectionWinModes');
const rulesGoalEl = document.getElementById('rulesGoal');
const rulesBoardEl = document.getElementById('rulesBoard');
const rulesPiecesEl = document.getElementById('rulesPieces');
const rulesMoveEl = document.getElementById('rulesMove');
const rulesCarryEl = document.getElementById('rulesCarry');
const rulesHeroEl = document.getElementById('rulesHero');
const rulesModesEl = document.getElementById('rulesModes');
const rulesDetailsSummaryEl = document.getElementById('rulesDetailsSummary');
const rulesDetail1El = document.getElementById('rulesDetail1');
const rulesDetail2El = document.getElementById('rulesDetail2');
const rulesDetail3El = document.getElementById('rulesDetail3');
const rulesDetail4El = document.getElementById('rulesDetail4');
const rulesDetail5El = document.getElementById('rulesDetail5');
const rulesDetail6El = document.getElementById('rulesDetail6');
const rulesDetail7El = document.getElementById('rulesDetail7');
const rulesDetail8El = document.getElementById('rulesDetail8');
const rulesDetail9El = document.getElementById('rulesDetail9');
const rulesDetail10El = document.getElementById('rulesDetail10');
const rulesDetail11El = document.getElementById('rulesDetail11');
const rulesDetail12El = document.getElementById('rulesDetail12');
const rulesDetailNoteEl = document.getElementById('rulesDetailNote');
const rulesPiecesSummaryEl = document.getElementById('rulesPiecesSummary');
const pieceRuleCEl = document.getElementById('pieceRuleC');
const pieceRuleHEl = document.getElementById('pieceRuleH');
const pieceRuleInEl = document.getElementById('pieceRuleIn');
const pieceRuleMEl = document.getElementById('pieceRuleM');
const pieceRuleTEl = document.getElementById('pieceRuleT');
const pieceRuleEEl = document.getElementById('pieceRuleE');
const pieceRuleAEl = document.getElementById('pieceRuleA');
const pieceRuleAaEl = document.getElementById('pieceRuleAa');
const pieceRuleMsEl = document.getElementById('pieceRuleMs');
const pieceRuleAfEl = document.getElementById('pieceRuleAf');
const pieceRuleNEl = document.getElementById('pieceRuleN');
const pieceRuleHeroEl = document.getElementById('pieceRuleHero');
const pieceRuleCarryEl = document.getElementById('pieceRuleCarry');
const openRulesDocBtn = document.getElementById('openRulesDocBtn');
const rulesDocModalEl = document.getElementById('rulesDocModal');
const closeRulesDocBtn = document.getElementById('closeRulesDocBtn');
const rulesDocTitleEl = document.getElementById('rulesDocTitle');
const rulesDocHintEl = document.getElementById('rulesDocHint');
const rulesDocDirectLinkEl = document.getElementById('rulesDocDirectLink');
const rulesDocFrameEl = document.getElementById('rulesDocFrame');
const rulesDocCardEl = rulesDocModalEl ? rulesDocModalEl.querySelector('.rules-doc-card') : null;
const startModeBtn = document.getElementById('startModeBtn');
const mainKickerEl = document.getElementById('mainKicker');
const riverLabelEl = document.getElementById('riverLabel');
const controlDeckTitleEl = document.getElementById('controlDeckTitle');
const sideFieldLabelEl = document.getElementById('sideFieldLabel');
const difficultyFieldLabelEl = document.getElementById('difficultyFieldLabel');
const themeFieldLabelEl = document.getElementById('themeFieldLabel');
const settingsTitleEl = document.getElementById('settingsTitle');
const boardSkinLabelEl = document.getElementById('boardSkinLabel');
const pieceThemeLabelEl = document.getElementById('pieceThemeLabel');
const soundToggleLabelEl = document.getElementById('soundToggleLabel');
const soundVolumeLabelEl = document.getElementById('soundVolumeLabel');
const animToggleLabelEl = document.getElementById('animToggleLabel');
const moveHintsLabelEl = document.getElementById('moveHintsLabel');
const telemetryTitleEl = document.getElementById('telemetryTitle');
const statLabelYouEl = document.getElementById('statLabelYou');
const statLabelDifficultyEl = document.getElementById('statLabelDifficulty');
const statLabelTurnEl = document.getElementById('statLabelTurn');
const statLabelLegalEl = document.getElementById('statLabelLegal');
const statLabelPiecesEl = document.getElementById('statLabelPieces');
const statLabelSelectionEl = document.getElementById('statLabelSelection');
const historyTitleEl = document.getElementById('historyTitle');
const legendTitleEl = document.getElementById('legendTitle');
const legendLegalTextEl = document.getElementById('legendLegalText');
const legendSelectedTextEl = document.getElementById('legendSelectedText');
const legendCaptureTextEl = document.getElementById('legendCaptureText');
const legendLastMoveTextEl = document.getElementById('legendLastMoveText');
const legendHeroTextEl = document.getElementById('legendHeroText');
const aboutTitleEl = document.getElementById('aboutTitle');
const aboutNameLabelEl = document.getElementById('aboutNameLabel');
const aboutPositionLabelEl = document.getElementById('aboutPositionLabel');
const aboutPositionValueEl = document.getElementById('aboutPositionValue');
const aboutGithubLabelEl = document.getElementById('aboutGithubLabel');
const aboutEmailLabelEl = document.getElementById('aboutEmailLabel');
const authPanelEl = document.getElementById('authPanel');
const authUserLabelEl = document.getElementById('authUserLabel');
const signInBtn = document.getElementById('signInBtn');
const signOutBtn = document.getElementById('signOutBtn');
const tutorialBtn = document.getElementById('tutorialBtn');
const quickTutorialOverlayEl = document.getElementById('quickTutorialOverlay');
const quickTutorialStartBtn = document.getElementById('quickTutorialStartBtn');
const quickTutorialSkipBtn = document.getElementById('quickTutorialSkipBtn');
const tutorialModalEl = document.getElementById('tutorialModal');
const tutorialCardEl = tutorialModalEl ? tutorialModalEl.querySelector('.tutorial-card') : null;
const tutorialKickerEl = document.getElementById('tutorialKicker');
const tutorialTitleEl = document.getElementById('tutorialTitle');
const tutorialProgressEl = document.getElementById('tutorialProgress');
const tutorialTextEl = document.getElementById('tutorialText');
const tutorialBoardEl = document.getElementById('tutorialBoard');
const skipTutorialBtn = document.getElementById('skipTutorialBtn');
const tutorialPrevBtn = document.getElementById('tutorialPrevBtn');
const tutorialReplayBtn = document.getElementById('tutorialReplayBtn');
const tutorialStartBtn = document.getElementById('tutorialStartBtn');
const tutorialNextBtn = document.getElementById('tutorialNextBtn');
const postGameModalEl = document.getElementById('postGameModal');
const postGameCardEl = document.getElementById('postGameCard');
const postGameTitleEl = document.getElementById('postGameTitle');
const postGameResultEl = document.getElementById('postGameResult');
const postGameStatsEl = document.getElementById('postGameStats');
const postGameShareStatusEl = document.getElementById('postGameShareStatus');
const postGameRematchBtn = document.getElementById('postGameRematchBtn');
const postGameShareBtn = document.getElementById('postGameShareBtn');
const postGameCloseBtn = document.getElementById('postGameCloseBtn');
const pwaInstallBtn = document.getElementById('pwaInstallBtn');

let gameId = null;
let state = null;
let selectedPid = null;
let botThinking = false;
let autoBotTimer = null;

let selectedLanguage = 'en';
let selectedPlayerMode = 'single';
let selectedMode = 'full';
let selectedSide = 'red';
let selectedDifficulty = 'medium';
let selectedTheme = 'system';
let boardFlipped = false;
let boardScale = 1;
let boardPinch = null;
let setupRulesMenuOpen = false;
let setupDetailsVisible = false;
let startGamePending = false;
let pendingRetryAction = null;
let hasStartedGame = false;
let guestMovePending = false;
let authUser = null;
let authUnsub = null;
let authBusy = false;
let authAvailable = true;
let googleSignInEnabled = false;

let firebaseClient = null;
let firebaseClientPromise = null;
let rtcFactory = null;
let rtcFactoryPromise = null;

let onlineMatchUnsub = null;
let onlineSession = {
  matchId: '',
  role: '',
  hostUid: '',
  guestUid: '',
  status: 'idle',
  link: ''
};
let onlineRtc = null;
let onlineStatusText = '';
let onlineQueryMatchCode = new URLSearchParams(window.location.search).get('match') || '';

let uiPrefs = { ...DEFAULT_UI_PREFS };
let boardFx = null;
let boardFxTimer = null;
let moveDelightTimer = null;
let moveDelightExplosionTimer = null;
let vectorSpriteCache = Object.create(null);

let spriteMap = Object.create(null);
let lastMoveFrom = null;
let moveHistory = [];
let stateHistory = [];
let reviewIndex = -1; // -1 = live board, otherwise stateHistory index

let lastRulesDocTrigger = null;
let lastSetupAboutTrigger = null;
let pendingRulesDocCloseAction = null;
let pendingTutorialCloseAction = null;
let tutorialStep = 0;
let tutorialPieces = TUTORIAL_INITIAL_PIECES.map((piece) => ({ ...piece }));
let tutorialAllowedMoves = [];
let tutorialActive = false;
let tutorialDrag = null;
let tutorialFocusCell = null;
let postGameShownSignature = '';
let deferredInstallPrompt = null;
let replayLoading = false;

let audioCtx = null;
let audioMaster = null;
let audioBuffers = Object.create(null);

function ensureAudio() {
  if (audioCtx) return audioCtx;
  const Ctx = window.AudioContext || window.webkitAudioContext;
  if (!Ctx) return null;
  try {
    audioCtx = new Ctx({ sampleRate: AUDIO_SAMPLE_RATE });
  } catch (_) {
    audioCtx = new Ctx();
  }
  audioMaster = audioCtx.createGain();
  audioMaster.gain.value = uiPrefs.soundEnabled ? uiPrefs.soundVolume : 0;
  audioMaster.connect(audioCtx.destination);
  return audioCtx;
}

function unlockAudio() {
  const ctx = ensureAudio();
  if (!ctx) return;
  if (ctx.state === 'suspended') ctx.resume().catch(() => {});
}

function buildSoundBuffer(name) {
  if (!audioCtx) return null;
  if (audioBuffers[name]) return audioBuffers[name];
  const profile = SOUND_LIBRARY[name];
  if (!profile) return null;

  const totalSamples = profile.notes.reduce(
    (acc, n) => acc + Math.max(1, Math.floor(AUDIO_SAMPLE_RATE * n.dur)),
    0
  );
  const data = new Float32Array(totalSamples);
  let offset = 0;

  profile.notes.forEach((note) => {
    const count = Math.max(1, Math.floor(AUDIO_SAMPLE_RATE * note.dur));
    for (let i = 0; i < count; i++) {
      const t = i / AUDIO_SAMPLE_RATE;
      const env = Math.exp(-t * note.decay);
      let v = 0;
      if (note.freq > 0) {
        const w = 2 * Math.PI * note.freq * t;
        v = Math.sin(w) * 0.7 + Math.sin(w * 2) * 0.2 + Math.sin(w * 0.5) * 0.1;
      }
      const s = profile.volume * env * v;
      data[offset + i] = Math.max(-1, Math.min(1, s));
    }
    offset += count;
  });

  const buf = audioCtx.createBuffer(1, totalSamples, AUDIO_SAMPLE_RATE);
  buf.getChannelData(0).set(data);
  audioBuffers[name] = buf;
  return buf;
}

function playSound(name) {
  if (!uiPrefs.soundEnabled) return;
  const ctx = ensureAudio();
  if (!ctx || ctx.state !== 'running') return;
  const buffer = buildSoundBuffer(name);
  if (!buffer || !audioMaster) return;
  const src = ctx.createBufferSource();
  src.buffer = buffer;
  src.connect(audioMaster);
  src.start();
}

function key(c, r) {
  return `${c},${r}`;
}

function isSea(c) {
  return c <= 2;
}

function isRiverRow(r) {
  return r === 5 || r === 6;
}

function isReef(c, r) {
  return isRiverRow(r) && (c === 5 || c === 7);
}

function terrainClass(c, r) {
  if (isRiverRow(r)) return 'river';
  if (isSea(c)) return 'sea';
  return 'land';
}

function activeLanguagePack() {
  return I18N[selectedLanguage] || I18N.en;
}

function textMap(key) {
  const localized = activeLanguagePack()[key];
  if (localized && typeof localized === 'object') return localized;
  const fallback = I18N.en[key];
  if (fallback && typeof fallback === 'object') return fallback;
  return Object.create(null);
}

function formatTemplate(template, vars = null) {
  if (!vars) return template;
  return template.replace(/\{(\w+)\}/g, (_, name) => {
    if (Object.prototype.hasOwnProperty.call(vars, name)) return String(vars[name]);
    return `{${name}}`;
  });
}

function t(key, vars = null) {
  const localized = activeLanguagePack()[key];
  const fallback = I18N.en[key];
  const template = typeof localized === 'string'
    ? localized
    : (typeof fallback === 'string' ? fallback : key);
  return formatTemplate(template, vars);
}

function errorMessage(error) {
  if (typeof error === 'string') return error.trim();
  if (error && typeof error.message === 'string') return error.message.trim();
  return '';
}

function isEngineConnectionErrorMessage(msg) {
  const text = String(msg || '').toLowerCase();
  if (!text) return false;
  if (/\bhttp\s*5\d{2}\b/.test(text)) return true;
  return (
    text.includes('failed to fetch') ||
    text.includes('networkerror') ||
    text.includes('network error') ||
    text.includes('load failed') ||
    text.includes('timeout') ||
    text.includes('engine bridge unavailable') ||
    text.includes('engine worker') ||
    text.includes('cannot connect') ||
    text.includes('wasm')
  );
}

function authFriendlyErrorMessage(msg) {
  const text = String(msg || '').toLowerCase();
  if (!text) return '';
  if (text.includes('auth/configuration-not-found') || text.includes('configuration_not_found')) {
    return t('authConfigMissing');
  }
  if (text.includes('auth/operation-not-allowed')) {
    return t('authGoogleNotEnabled');
  }
  return '';
}

function setRetryAction(action = null) {
  pendingRetryAction = (typeof action === 'function') ? action : null;
  if (!retryBtn) return;
  retryBtn.hidden = !pendingRetryAction;
  retryBtn.disabled = false;
}

function clearRetryAction() {
  setRetryAction(null);
}

function updateStartModeButton() {
  if (!startModeBtn) return;
  startModeBtn.disabled = startGamePending;
  startModeBtn.classList.toggle('is-loading', startGamePending);
  startModeBtn.textContent = startGamePending ? t('startingGame') : t('startGame');
  if (quickRestartBtn) quickRestartBtn.disabled = startGamePending;
}

function updateQuickRestartVisibility() {
  if (!quickRestartBtn) return;
  quickRestartBtn.hidden = !hasStartedGame;
}

function announceStatusLive() {
  if (!statusLiveEl) return;
  const combined = `${statusEl.textContent}. ${metaEl.textContent}`.trim();
  if (!combined) return;
  if (statusLiveEl.textContent === combined) {
    statusLiveEl.textContent = '';
    window.setTimeout(() => {
      statusLiveEl.textContent = combined;
    }, 0);
    return;
  }
  statusLiveEl.textContent = combined;
}

function isValidTheme(theme) {
  return THEME_KEYS.includes(theme);
}

function systemPrefersDarkTheme() {
  return !!(window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches);
}

function resolvedTheme(theme) {
  if (theme === 'dark') return 'dark';
  if (theme === 'light') return 'light';
  return systemPrefersDarkTheme() ? 'dark' : 'light';
}

function readStoredTheme() {
  try {
    const raw = window.localStorage.getItem(THEME_STORAGE_KEY);
    return isValidTheme(raw) ? raw : null;
  } catch (_) {
    return null;
  }
}

function storeTheme(theme) {
  try {
    window.localStorage.setItem(THEME_STORAGE_KEY, theme);
  } catch (_) {
    // ignore storage failures (private mode / restricted env)
  }
}

function applyTheme(theme = selectedTheme, persist = false) {
  selectedTheme = isValidTheme(theme) ? theme : 'system';
  const effectiveTheme = resolvedTheme(selectedTheme);
  document.documentElement.setAttribute('data-theme', effectiveTheme);
  if (themeSelect && themeSelect.value !== selectedTheme) themeSelect.value = selectedTheme;
  if (persist) storeTheme(selectedTheme);
}

function initializeThemePreference() {
  selectedTheme = readStoredTheme() || 'system';
  applyTheme(selectedTheme, false);
  if (window.matchMedia) {
    const mql = window.matchMedia('(prefers-color-scheme: dark)');
    const onChange = () => {
      if (selectedTheme === 'system') applyTheme('system', false);
    };
    if (typeof mql.addEventListener === 'function') mql.addEventListener('change', onChange);
    else if (typeof mql.addListener === 'function') mql.addListener(onChange);
  }
}

function isValidBoardSkin(boardSkin) {
  return BOARD_SKIN_KEYS.includes(boardSkin);
}

function isValidPieceTheme(pieceTheme) {
  return PIECE_THEME_KEYS.includes(pieceTheme);
}

function clampVolume(raw) {
  const n = Number(raw);
  if (!Number.isFinite(n)) return DEFAULT_UI_PREFS.soundVolume;
  return Math.max(0, Math.min(1, n));
}

function normalizeUiPrefs(raw = null) {
  const src = raw && typeof raw === 'object' ? raw : Object.create(null);
  return {
    boardSkin: isValidBoardSkin(src.boardSkin) ? src.boardSkin : DEFAULT_UI_PREFS.boardSkin,
    pieceTheme: isValidPieceTheme(src.pieceTheme) ? src.pieceTheme : DEFAULT_UI_PREFS.pieceTheme,
    soundEnabled: typeof src.soundEnabled === 'boolean' ? src.soundEnabled : DEFAULT_UI_PREFS.soundEnabled,
    soundVolume: clampVolume(src.soundVolume),
    animationsEnabled: typeof src.animationsEnabled === 'boolean' ? src.animationsEnabled : DEFAULT_UI_PREFS.animationsEnabled,
    moveHints: typeof src.moveHints === 'boolean' ? src.moveHints : DEFAULT_UI_PREFS.moveHints
  };
}

function readStoredUiPrefs() {
  try {
    const raw = window.localStorage.getItem(UI_PREF_STORAGE_KEY);
    if (!raw) return { ...DEFAULT_UI_PREFS };
    return normalizeUiPrefs(JSON.parse(raw));
  } catch (_) {
    return { ...DEFAULT_UI_PREFS };
  }
}

function storeUiPrefs(prefs) {
  try {
    window.localStorage.setItem(UI_PREF_STORAGE_KEY, JSON.stringify(prefs));
  } catch (_) {
    // ignore storage failures
  }
}

function updateAudioMasterGain() {
  if (!audioMaster) return;
  audioMaster.gain.value = uiPrefs.soundEnabled ? uiPrefs.soundVolume : 0;
}

function ensureMoveDelightLayer() {
  if (!boardFrameEl) return null;
  let layer = boardFrameEl.querySelector('.move-fx-layer');
  if (!layer) {
    layer = document.createElement('div');
    layer.className = 'move-fx-layer';
    boardFrameEl.appendChild(layer);
  }
  return layer;
}

function boardCellCenter(c, r) {
  if (!boardEl || !boardFrameEl) return null;
  const cell = boardEl.querySelector(`.cell[data-col="${c}"][data-row="${r}"]`);
  if (!cell) return null;
  const cellRect = cell.getBoundingClientRect();
  const frameRect = boardFrameEl.getBoundingClientRect();
  return {
    x: (cellRect.left - frameRect.left) + cellRect.width * 0.5,
    y: (cellRect.top - frameRect.top) + cellRect.height * 0.5,
    size: Math.min(cellRect.width, cellRect.height) * 0.82
  };
}

function clearMoveDelightFx() {
  if (moveDelightTimer) {
    clearTimeout(moveDelightTimer);
    moveDelightTimer = null;
  }
  if (moveDelightExplosionTimer) {
    clearTimeout(moveDelightExplosionTimer);
    moveDelightExplosionTimer = null;
  }
  if (!boardFrameEl) return;
  const layer = boardFrameEl.querySelector('.move-fx-layer');
  if (layer) layer.innerHTML = '';
}

function spawnCaptureExplosion(c, r, player = 'red') {
  if (!uiPrefs.animationsEnabled) return;
  const center = boardCellCenter(c, r);
  const layer = ensureMoveDelightLayer();
  if (!center || !layer) return;

  const burst = document.createElement('div');
  burst.className = `move-fx-explosion ${player === 'blue' ? 'fx-blue' : 'fx-red'}`;
  burst.style.left = `${center.x}px`;
  burst.style.top = `${center.y}px`;

  for (let i = 0; i < 8; i++) {
    const p = document.createElement('span');
    const angle = (Math.PI * 2 * i) / 8;
    const dist = 14 + Math.random() * 16;
    p.style.setProperty('--dx', `${Math.cos(angle) * dist}px`);
    p.style.setProperty('--dy', `${Math.sin(angle) * dist}px`);
    p.style.setProperty('--delay', `${Math.random() * 70}ms`);
    burst.appendChild(p);
  }

  layer.appendChild(burst);
  moveDelightExplosionTimer = window.setTimeout(() => {
    burst.remove();
  }, 720);
}

function runMoveDelight(prevState, nextState, fromSquare) {
  if (!uiPrefs.animationsEnabled) return;
  if (!prevState || !nextState || !nextState.has_last_move || !nextState.last_move) return;

  clearMoveDelightFx();

  const toSquare = {
    c: Number(nextState.last_move.dc),
    r: Number(nextState.last_move.dr)
  };
  if (!Number.isFinite(toSquare.c) || !Number.isFinite(toSquare.r)) return;

  const pid = Number(nextState.last_move.pid);
  const prevPiece = prevState.pieces.find((p) => p.id === pid && p.carrier_id < 0) || null;
  const nextPiece = nextState.pieces.find((p) => p.id === pid && p.carrier_id < 0) || null;
  const movedPiece = nextPiece || prevPiece;
  if (!movedPiece) return;

  const layer = ensureMoveDelightLayer();
  if (!layer) return;

  const fromCenter = fromSquare ? boardCellCenter(fromSquare.c, fromSquare.r) : null;
  const toCenter = boardCellCenter(toSquare.c, toSquare.r);
  if (!toCenter) return;

  if (!fromCenter) {
    if (nextState.last_move_capture) spawnCaptureExplosion(toSquare.c, toSquare.r, nextState.last_move_player || 'red');
    return;
  }

  const sameSquare = (fromSquare.c === toSquare.c && fromSquare.r === toSquare.r);
  const token = renderPieceToken(movedPiece);
  token.classList.add(
    'move-fx-piece',
    `kind-${kindCssToken(movedPiece.kind)}`,
    movedPiece.player === 'blue' ? 'fx-blue' : 'fx-red'
  );
  token.style.width = `${fromCenter.size}px`;
  token.style.height = `${fromCenter.size}px`;
  token.style.left = `${fromCenter.x}px`;
  token.style.top = `${fromCenter.y}px`;
  layer.appendChild(token);

  let trail = null;
  if (!sameSquare && (movedPiece.kind === 'Af' || movedPiece.kind === 'N')) {
    trail = document.createElement('div');
    trail.className = `move-fx-trail ${movedPiece.kind === 'Af' ? 'trail-jet' : 'trail-wake'}`;
    const dx = toCenter.x - fromCenter.x;
    const dy = toCenter.y - fromCenter.y;
    const len = Math.max(12, Math.hypot(dx, dy));
    const angle = Math.atan2(dy, dx) * (180 / Math.PI);
    trail.style.left = `${fromCenter.x}px`;
    trail.style.top = `${fromCenter.y}px`;
    trail.style.width = `${len}px`;
    trail.style.transform = `translateY(-50%) rotate(${angle}deg)`;
    layer.appendChild(trail);
  }

  window.requestAnimationFrame(() => {
    if (sameSquare) {
      token.classList.add('in-place');
      return;
    }
    const dx = toCenter.x - fromCenter.x;
    const dy = toCenter.y - fromCenter.y;
    token.classList.add('in-flight');
    token.style.transform = `translate(-50%, -50%) translate(${dx}px, ${dy}px)`;
  });

  if (nextState.last_move_capture) {
    const delay = sameSquare ? 100 : 300;
    moveDelightExplosionTimer = window.setTimeout(() => {
      spawnCaptureExplosion(toSquare.c, toSquare.r, nextState.last_move_player || movedPiece.player || 'red');
    }, delay);
  }

  moveDelightTimer = window.setTimeout(() => {
    token.remove();
    if (trail) trail.remove();
  }, 740);
}

function clearBoardFx() {
  boardFx = null;
  if (boardFxTimer) {
    clearTimeout(boardFxTimer);
    boardFxTimer = null;
  }
  clearMoveDelightFx();
}

function scheduleBoardFxClear(ms = 720) {
  if (boardFxTimer) clearTimeout(boardFxTimer);
  boardFxTimer = window.setTimeout(() => {
    boardFx = null;
    boardFxTimer = null;
    drawBoard();
  }, ms);
}

function rememberBoardFx(nextState, fromSquare) {
  if (!nextState || !nextState.has_last_move || !nextState.last_move || !uiPrefs.animationsEnabled) {
    clearBoardFx();
    return;
  }
  boardFx = {
    from: fromSquare || null,
    to: { c: nextState.last_move.dc, r: nextState.last_move.dr },
    capture: !!nextState.last_move_capture,
    player: nextState.last_move_player || 'red'
  };
  scheduleBoardFxClear();
}

function applyUiPrefs(nextPrefs = uiPrefs, persist = false) {
  uiPrefs = normalizeUiPrefs(nextPrefs);

  document.documentElement.setAttribute('data-board-skin', uiPrefs.boardSkin);
  document.documentElement.setAttribute('data-piece-theme', uiPrefs.pieceTheme);
  document.documentElement.setAttribute('data-animations', uiPrefs.animationsEnabled ? 'on' : 'off');
  if (boardEl) boardEl.classList.toggle('no-hints', !uiPrefs.moveHints);

  if (boardSkinSelect && boardSkinSelect.value !== uiPrefs.boardSkin) {
    boardSkinSelect.value = uiPrefs.boardSkin;
  }
  if (pieceThemeSelect && pieceThemeSelect.value !== uiPrefs.pieceTheme) {
    pieceThemeSelect.value = uiPrefs.pieceTheme;
  }
  if (soundToggleEl) soundToggleEl.checked = uiPrefs.soundEnabled;
  if (soundVolumeRangeEl) {
    soundVolumeRangeEl.value = String(Math.round(uiPrefs.soundVolume * 100));
    soundVolumeRangeEl.disabled = !uiPrefs.soundEnabled;
  }
  if (animToggleEl) animToggleEl.checked = uiPrefs.animationsEnabled;
  if (moveHintsToggleEl) moveHintsToggleEl.checked = uiPrefs.moveHints;

  if (!uiPrefs.animationsEnabled) clearBoardFx();
  updateAudioMasterGain();

  if (persist) storeUiPrefs(uiPrefs);
}

function initializeUiPrefs() {
  uiPrefs = readStoredUiPrefs();
  applyUiPrefs(uiPrefs, false);
}

function isValidMode(mode) {
  return MODE_KEYS.includes(mode);
}

function isValidDifficulty(difficulty) {
  return DIFFICULTY_KEYS.includes(difficulty);
}

function isValidLanguage(language) {
  return LANGUAGE_KEYS.includes(language);
}

function isValidPlayerMode(playerMode) {
  return PLAYER_MODE_KEYS.includes(playerMode);
}

function isLocalMultiplayer() {
  return selectedPlayerMode === 'local';
}

function isOnlineMultiplayer() {
  return selectedPlayerMode === 'online';
}

function onlineRoleSide() {
  if (onlineSession.role === 'host') return 'red';
  if (onlineSession.role === 'guest') return 'blue';
  return 'red';
}

function onlineRoleLabel() {
  if (onlineSession.role === 'host') return t('onlineRoleHost');
  if (onlineSession.role === 'guest') return t('onlineRoleGuest');
  return t('onlineRoleUnknown');
}

function sanitizeMatchId(raw) {
  return String(raw || '')
    .trim()
    .replace(/\s+/g, '')
    .replace(/[^a-zA-Z0-9_-]/g, '');
}

function buildOnlineMatchLink(matchId) {
  if (!matchId) return '';
  const url = new URL(window.location.href);
  url.searchParams.set('match', matchId);
  return url.toString();
}

function setOnlineStatus(key, vars = null, force = false) {
  const next = t(key, vars);
  if (!force && onlineStatusText === next) return;
  onlineStatusText = next;
  if (onlineStatusTextEl) {
    onlineStatusTextEl.textContent = onlineStatusText;
  }
}

function clearOnlineMatchSubscription() {
  if (onlineMatchUnsub) {
    onlineMatchUnsub();
    onlineMatchUnsub = null;
  }
}

function closeOnlineRtc() {
  if (onlineRtc) {
    try {
      onlineRtc.close();
    } catch (_) {
      // ignore
    }
    onlineRtc = null;
  }
}

function clearOnlineSession(keepQueryCode = false) {
  clearOnlineMatchSubscription();
  closeOnlineRtc();
  onlineSession = {
    matchId: '',
    role: '',
    hostUid: '',
    guestUid: '',
    status: 'idle',
    link: ''
  };
  guestMovePending = false;
  if (!keepQueryCode) onlineQueryMatchCode = '';
  if (onlineMatchCodeEl) onlineMatchCodeEl.value = '-';
  setOnlineStatus('onlineStatusIdle', null, true);
  updateOnlineStatusUI();
}

function updateOnlinePanelVisibility() {
  if (!onlinePanelEl) return;
  onlinePanelEl.hidden = !isOnlineMultiplayer();
}

function updateOnlineStatusUI() {
  updateOnlinePanelVisibility();
  if (!onlinePanelEl || onlinePanelEl.hidden) return;

  if (onlineMatchCodeEl) {
    onlineMatchCodeEl.value = onlineSession.matchId || '-';
  }

  if (joinMatchInputEl && onlineQueryMatchCode && !joinMatchInputEl.value) {
    joinMatchInputEl.value = onlineQueryMatchCode;
  }

  const hasMatch = !!onlineSession.matchId;
  const canCopy = hasMatch && !!onlineSession.link;

  if (copyOnlineCodeBtn) copyOnlineCodeBtn.disabled = !hasMatch;
  if (copyOnlineLinkBtn) copyOnlineLinkBtn.disabled = !canCopy;
  if (createOnlineBtn) createOnlineBtn.disabled = startGamePending;
  if (joinOnlineBtn) joinOnlineBtn.disabled = startGamePending;

  const statusText = onlineStatusText || t('onlineStatusIdle');
  if (onlineStatusTextEl) {
    const roleSuffix = hasMatch ? ` ${t('onlineRoleLabel', { role: onlineRoleLabel() })}` : '';
    onlineStatusTextEl.textContent = `${statusText}${roleSuffix}`;
  }
}

function authDisplayName(user = authUser) {
  if (!user) return '';
  const display = typeof user.displayName === 'string' ? user.displayName.trim() : '';
  if (display) return display;
  const email = typeof user.email === 'string' ? user.email.trim() : '';
  if (email) return email;
  const uid = String(user.uid || '').trim();
  if (uid) return `UID-${uid.slice(0, 8)}`;
  return 'Commander';
}

function isLinkedAccountUser(user = authUser) {
  return !!(user && !user.isAnonymous);
}

function updateAuthUI() {
  if (!authPanelEl || !authUserLabelEl || !signInBtn || !signOutBtn) return;

  if (!authAvailable) {
    authUserLabelEl.textContent = t('authUnavailable');
    signInBtn.hidden = true;
    signOutBtn.hidden = true;
    signInBtn.disabled = true;
    signOutBtn.disabled = true;
    return;
  }

  signInBtn.disabled = authBusy;
  signOutBtn.disabled = authBusy;

  if (!googleSignInEnabled && !isLinkedAccountUser()) {
    authUserLabelEl.textContent = t('authGuest');
    signInBtn.hidden = true;
    signOutBtn.hidden = true;
    return;
  }

  if (isLinkedAccountUser()) {
    authUserLabelEl.textContent = t('authSignedInAs', { name: authDisplayName() });
    signInBtn.hidden = true;
    signOutBtn.hidden = false;
    return;
  }

  authUserLabelEl.textContent = t('authGuest');
  signInBtn.hidden = false;
  signOutBtn.hidden = true;
}

function setAuthBusy(value) {
  authBusy = !!value;
  updateAuthUI();
}

async function ensureCommanderUserProfileDoc(user) {
  if (!user || user.isAnonymous) return;
  const fc = await ensureFirebaseClient();
  if (!fc.firebaseReady) return;

  const userRef = fc.doc(fc.db, 'users', user.uid);
  const snap = await fc.getDoc(userRef);
  const displayName = authDisplayName(user);
  const photoURL = (typeof user.photoURL === 'string') ? user.photoURL : '';

  if (!snap.exists()) {
    await fc.setDoc(userRef, {
      uid: user.uid,
      displayName,
      photoURL,
      createdAt: fc.serverTimestamp(),
      elo: 1200,
      wins: 0,
      losses: 0,
      draws: 0,
      games: 0
    });
    return;
  }

  await fc.updateDoc(userRef, { displayName, photoURL });
}

async function initializeAuth() {
  updateAuthUI();
  try {
    const fc = await ensureFirebaseClient();
    if (!fc.firebaseReady) {
      authAvailable = false;
      updateAuthUI();
      return;
    }

    authAvailable = true;
    googleSignInEnabled = !!fc.googleSignInEnabled;
    if (authUnsub) authUnsub();
    authUnsub = fc.onAuthState(async (user) => {
      authUser = user;
      updateAuthUI();
      if (user && !user.isAnonymous) {
        try {
          await ensureCommanderUserProfileDoc(user);
        } catch (err) {
          console.warn('[Commander Chess] user profile sync failed', err);
        }
      }
    });
    updateAuthUI();
  } catch (err) {
    console.warn('[Commander Chess] auth init failed', err);
    authAvailable = false;
    updateAuthUI();
  }
}

async function handleGoogleSignIn() {
  if (!googleSignInEnabled) throw new Error(t('authGoogleNotEnabled'));
  setAuthBusy(true);
  try {
    const fc = await ensureFirebaseClient();
    if (!fc.firebaseReady) throw new Error(t('authUnavailable'));
    const user = await fc.signInWithGoogle();
    authUser = user;
    await ensureCommanderUserProfileDoc(user);
    updateAuthUI();
  } finally {
    setAuthBusy(false);
  }
}

async function handleSignOut() {
  if (isOnlineMultiplayer() && onlineSession.matchId && state && !state.game_over) {
    const proceed = window.confirm(t('authSignOutConfirm'));
    if (!proceed) return;
  }

  setAuthBusy(true);
  try {
    const fc = await ensureFirebaseClient();
    if (!fc.firebaseReady) throw new Error(t('authUnavailable'));
    await fc.signOutUser();
    authUser = null;
    updateAuthUI();
  } finally {
    setAuthBusy(false);
  }
}

async function copyTextToClipboard(value, successKey) {
  const text = String(value || '').trim();
  if (!text) return;
  try {
    if (navigator.clipboard && typeof navigator.clipboard.writeText === 'function') {
      await navigator.clipboard.writeText(text);
      setOnlineStatus(successKey, null, true);
      return;
    }
  } catch (_) {
    // fallback path below
  }

  try {
    const area = document.createElement('textarea');
    area.value = text;
    area.style.position = 'fixed';
    area.style.left = '-1000px';
    area.style.top = '-1000px';
    document.body.appendChild(area);
    area.focus();
    area.select();
    const ok = document.execCommand('copy');
    document.body.removeChild(area);
    if (ok) setOnlineStatus(successKey, null, true);
    else setOnlineStatus('onlineClipboardFailed', null, true);
  } catch (_) {
    setOnlineStatus('onlineClipboardFailed', null, true);
  }
}

async function ensureFirebaseClient() {
  if (firebaseClient) return firebaseClient;
  if (firebaseClientPromise) return firebaseClientPromise;

  firebaseClientPromise = import('/js/firebaseClient.js')
    .then((mod) => {
      firebaseClient = mod;
      return firebaseClient;
    })
    .catch((err) => {
      firebaseClientPromise = null;
      throw err;
    });

  return firebaseClientPromise;
}

async function ensureRtcFactory() {
  if (rtcFactory) return rtcFactory;
  if (rtcFactoryPromise) return rtcFactoryPromise;

  rtcFactoryPromise = import('/js/rtc.js')
    .then((mod) => {
      rtcFactory = mod && typeof mod.createMatchRtcTransport === 'function'
        ? mod.createMatchRtcTransport
        : null;
      if (!rtcFactory) throw new Error('RTC module is unavailable');
      return rtcFactory;
    })
    .catch((err) => {
      rtcFactoryPromise = null;
      throw err;
    });

  return rtcFactoryPromise;
}

async function ensureOnlineReady() {
  const fc = await ensureFirebaseClient();
  if (!fc.firebaseReady) throw new Error(t('onlineFirestoreMissing'));
  await fc.ensureSignedIn();
  return fc;
}

function sideCaps(side) {
  const labels = textMap('sideCaps');
  return labels[side] || String(side || '').toUpperCase();
}

function playerModeLabel(playerMode = selectedPlayerMode, shortLabel = false) {
  const mapKey = shortLabel ? 'playerModeShortLabelByCode' : 'playerModeLabelByCode';
  const labels = textMap(mapKey);
  return labels[playerMode] || labels.single || playerMode;
}

function humanSide() {
  if (isOnlineMultiplayer()) return onlineRoleSide();
  if (isLocalMultiplayer()) {
    if (state && (state.turn === 'red' || state.turn === 'blue')) return state.turn;
    return 'red';
  }
  return sideSelect ? sideSelect.value : selectedSide;
}

function activeMode() {
  if (state && state.game_mode && isValidMode(state.game_mode)) return state.game_mode;
  return selectedMode;
}

function modeLabel(mode) {
  const labels = textMap('modeLabel');
  return labels[mode] || labels.full || mode;
}

function modeDescription(mode) {
  const labels = textMap('modeDesc');
  return labels[mode] || labels.full || '';
}

function activeDifficulty() {
  if (state && state.difficulty && isValidDifficulty(state.difficulty)) return state.difficulty;
  return selectedDifficulty;
}

function difficultyLabel(difficulty) {
  const labels = textMap('difficultyLabel');
  return labels[difficulty] || labels.medium || difficulty;
}

function difficultyDescription(difficulty) {
  const labels = textMap('difficultyDesc');
  return labels[difficulty] || labels.medium || '';
}

function boardCoordFromView(viewCol, viewRow) {
  return {
    c: boardFlipped ? (COLS - 1 - viewCol) : viewCol,
    r: boardFlipped ? viewRow : (ROWS - 1 - viewRow)
  };
}

function viewCoordFromBoard(col, row) {
  return {
    viewCol: boardFlipped ? (COLS - 1 - col) : col,
    viewRow: boardFlipped ? row : (ROWS - 1 - row)
  };
}

function boardIsZoomed() {
  return boardScale > 1.01;
}

function setBoardScale(nextScale, options = null) {
  const opts = options && typeof options === 'object' ? options : Object.create(null);
  const skipLabelUpdate = !!opts.skipLabelUpdate;
  const numeric = Number(nextScale);
  const clamped = Number.isFinite(numeric) ? Math.max(1, Math.min(2.6, numeric)) : 1;
  boardScale = clamped;
  if (boardStageEl) boardStageEl.classList.toggle('is-zoomed', boardIsZoomed());
  if (boardFrameEl) {
    boardFrameEl.style.setProperty('--board-scale', String(clamped));
  }
  if (!skipLabelUpdate) updateFlipButtonLabel();
}

function resetBoardViewState() {
  const needsReset = boardFlipped || boardIsZoomed();
  if (!needsReset) return false;
  boardFlipped = false;
  boardPinch = null;
  setBoardScale(1, { skipLabelUpdate: true });
  renderCoords();
  drawBoard();
  updateFlipButtonLabel();
  return true;
}

function touchDistance(a, b) {
  const dx = b.clientX - a.clientX;
  const dy = b.clientY - a.clientY;
  return Math.hypot(dx, dy);
}

function onBoardPinchStart(ev) {
  if (ev.touches.length !== 2) return;
  const [a, b] = ev.touches;
  const dist = touchDistance(a, b);
  if (!(dist > 0)) return;
  boardPinch = {
    startDistance: dist,
    startScale: boardScale
  };
}

function onBoardPinchMove(ev) {
  if (!boardPinch || ev.touches.length !== 2) return;
  const [a, b] = ev.touches;
  const dist = touchDistance(a, b);
  if (!(dist > 0)) return;
  ev.preventDefault();
  const nextScale = boardPinch.startScale * (dist / boardPinch.startDistance);
  setBoardScale(nextScale);
}

function onBoardPinchEnd(ev) {
  if (ev.touches.length < 2) {
    boardPinch = null;
  }
}

function updateFlipButtonLabel() {
  if (!flipBtn) return;
  flipBtn.textContent = (boardFlipped || boardIsZoomed()) ? t('resetBoardView') : t('flipBoard');
}

function rulesDocUrl() {
  return `${window.location.origin}/docs/Commander_Chess_Rules.docx`;
}

function rulesPageUrl() {
  return `${window.location.origin}/rules.html?lang=${encodeURIComponent(selectedLanguage)}`;
}

function rulesModalFocusableElements() {
  if (!rulesDocModalEl) return [];
  return Array.from(
    rulesDocModalEl.querySelectorAll(
      'button:not([disabled]), a[href], iframe, [tabindex]:not([tabindex="-1"])'
    )
  ).filter((el) => !el.hasAttribute('hidden'));
}

function trapRulesDocFocus(ev) {
  if (!rulesDocModalEl || !rulesDocModalEl.classList.contains('show')) return;

  if (ev.key === 'Escape') {
    ev.preventDefault();
    closeRulesDocModal();
    return;
  }

  if (ev.key !== 'Tab') return;

  const focusables = rulesModalFocusableElements();
  if (focusables.length === 0) {
    ev.preventDefault();
    return;
  }

  const first = focusables[0];
  const last = focusables[focusables.length - 1];
  const active = document.activeElement;

  if (ev.shiftKey) {
    if (active === first || !rulesDocModalEl.contains(active)) {
      ev.preventDefault();
      last.focus();
    }
  } else if (active === last || !rulesDocModalEl.contains(active)) {
    ev.preventDefault();
    first.focus();
  }
}

function openRulesDocModal(triggerEl = null) {
  if (!rulesDocModalEl) return;
  lastRulesDocTrigger = triggerEl || (document.activeElement instanceof HTMLElement ? document.activeElement : null);
  rulesDocModalEl.classList.add('show');
  rulesDocModalEl.setAttribute('aria-hidden', 'false');
  document.removeEventListener('keydown', trapRulesDocFocus);
  document.addEventListener('keydown', trapRulesDocFocus);
  if (rulesDocFrameEl) rulesDocFrameEl.src = rulesPageUrl();
  window.setTimeout(() => {
    if (closeRulesDocBtn) closeRulesDocBtn.focus();
    else if (rulesDocCardEl) rulesDocCardEl.focus();
  }, 0);
}

function closeRulesDocModal() {
  if (!rulesDocModalEl) return;
  rulesDocModalEl.classList.remove('show');
  rulesDocModalEl.setAttribute('aria-hidden', 'true');
  document.removeEventListener('keydown', trapRulesDocFocus);
  if (lastRulesDocTrigger && typeof lastRulesDocTrigger.focus === 'function') {
    lastRulesDocTrigger.focus();
  }
  lastRulesDocTrigger = null;
  const afterClose = pendingRulesDocCloseAction;
  pendingRulesDocCloseAction = null;
  if (typeof afterClose === 'function') {
    window.setTimeout(() => {
      afterClose();
    }, 0);
  }
}

function pieceAt(c, r, src = null) {
  const s = src || state;
  if (!s) return null;
  return s.pieces.find(p => p.col === c && p.row === r && p.carrier_id < 0) || null;
}

function pieceById(pid, src = null) {
  const s = src || state;
  if (!s || pid == null) return null;
  return s.pieces.find(p => p.id === pid && p.carrier_id < 0) || null;
}

function findLastMoveFrom(prevState, nextState) {
  if (!prevState || !nextState || !nextState.has_last_move || !nextState.last_move) return null;
  const pid = nextState.last_move.pid;
  if (typeof pid !== 'number') return null;
  const prevPiece = prevState.pieces.find(p => p.id === pid && p.carrier_id < 0);
  if (!prevPiece) return null;
  return { c: prevPiece.col, r: prevPiece.row };
}

function cloneState(src) {
  return src ? JSON.parse(JSON.stringify(src)) : null;
}

function currentViewState() {
  if (reviewIndex >= 0 && reviewIndex < stateHistory.length) return stateHistory[reviewIndex];
  return state;
}

function moveKey(s) {
  if (!s || !s.has_last_move || !s.last_move) return '';
  return `${s.last_move.pid}:${s.last_move.dc},${s.last_move.dr}:${s.last_move_player}:${s.last_move_capture ? 1 : 0}`;
}

function playTransitionSounds(prevState, nextState) {
  if (!prevState || !nextState) return;

  const prevMove = moveKey(prevState);
  const nextMove = moveKey(nextState);
  if (nextMove && nextMove !== prevMove && nextState.last_move) {
    playSound('radio');
    const pid = nextState.last_move.pid;
    const prevPiece = prevState.pieces.find(p => p.id === pid && p.carrier_id < 0) || null;
    const nextPiece = nextState.pieces.find(p => p.id === pid && p.carrier_id < 0) || null;
    const isCapture = !!nextState.last_move_capture;
    const kamikaze = !!prevPiece && prevPiece.kind === 'Af' && isCapture && !nextPiece;
    const movedKind = (nextPiece && nextPiece.kind) || (prevPiece && prevPiece.kind) || '';

    if (kamikaze) playSound('boom');
    else {
      if (movedKind === 'Af' || movedKind === 'N' || movedKind === 'T') playSound('rumble');
      playSound(isCapture ? 'capture' : 'move');
    }

    if (prevPiece && nextPiece && !prevPiece.hero && !!nextPiece.hero) {
      playSound('hero');
    }
  }

  if (!prevState.game_over && nextState.game_over) playSound('horn');
}

function addHistoryEntry(prevState, nextState, fromSquare) {
  if (!nextState || !nextState.has_last_move || !nextState.last_move) return;
  if (moveKey(prevState) === moveKey(nextState)) return;
  moveHistory.push({
    player: nextState.last_move_player,
    from: fromSquare || null,
    to: { c: nextState.last_move.dc, r: nextState.last_move.dr },
    capture: !!nextState.last_move_capture,
    pid: nextState.last_move.pid
  });
  stateHistory.push(cloneState(nextState));
  reviewIndex = -1;
}

function historyLabel(m, idx) {
  const from = m.from ? `(${m.from.c},${m.from.r})` : '(?)';
  const to = `(${m.to.c},${m.to.r})`;
  const cap = m.capture ? ' x' : '';
  return t('historyFormat', {
    index: idx + 1,
    player: sideCaps(m.player),
    from,
    to,
    capture: cap
  });
}

function updateHistoryUI() {
  historyListEl.innerHTML = '';

  if (moveHistory.length === 0) {
    const li = document.createElement('li');
    li.textContent = t('noMovesYet');
    historyListEl.appendChild(li);
  } else {
    moveHistory.forEach((m, idx) => {
      const li = document.createElement('li');
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.textContent = historyLabel(m, idx);
      const snapIdx = idx + 1; // state after this move
      if (reviewIndex === snapIdx) btn.classList.add('active');
      btn.addEventListener('click', () => {
        reviewIndex = snapIdx;
        selectedPid = null;
        updateHistoryUI();
        updateStatus();
        drawBoard();
      });
      li.appendChild(btn);
      historyListEl.appendChild(li);
    });
  }

  const hasHistory = stateHistory.length > 1;
  const reviewing = reviewIndex >= 0;
  histPrevBtn.disabled = !hasHistory || (reviewing && reviewIndex <= 0);
  histNextBtn.disabled = !reviewing;
  histLiveBtn.disabled = !reviewing;
}

function legalMovesForPid(pid, src = null) {
  const s = src || state;
  if (!s || pid == null) return [];
  return s.legal_moves.filter(m => m.pid === pid);
}

function renderCoords() {
  xCoordsEl.innerHTML = '';
  yCoordsEl.innerHTML = '';

  for (let viewCol = 0; viewCol < COLS; viewCol++) {
    const s = document.createElement('span');
    const boardPos = boardCoordFromView(viewCol, ROWS - 1);
    s.textContent = String(boardPos.c);
    xCoordsEl.appendChild(s);
  }

  for (let viewRow = 0; viewRow < ROWS; viewRow++) {
    const s = document.createElement('span');
    const boardPos = boardCoordFromView(0, viewRow);
    s.textContent = String(boardPos.r);
    yCoordsEl.appendChild(s);
  }
}

function spriteForPiece(piece) {
  const keyName = `${piece.kind}_${piece.player}`;
  const b64 = spriteMap[keyName];
  return b64 ? `data:image/png;base64,${b64}` : null;
}

function pieceGlyph(kind) {
  const code = PIECE_GLYPH[kind];
  if (code) return code;
  return String(kind || '?').slice(0, 2).toUpperCase();
}

function kindCssToken(kind) {
  return String(kind || '')
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, '-')
    .replace(/^-+|-+$/g, '') || 'unit';
}

function escapeSvgText(text) {
  return String(text)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function vectorSpriteForPiece(piece) {
  const keyName = `${piece.kind}_${piece.player}_${piece.hero ? 1 : 0}`;
  const cacheKey = `${uiPrefs.pieceTheme}:${keyName}`;
  if (vectorSpriteCache[cacheKey]) return vectorSpriteCache[cacheKey];
  const gradId = `bg-${cacheKey.replace(/[^a-zA-Z0-9_-]/g, '-')}`;

  const isRed = piece.player === 'red';
  const outer = isRed ? '#8e1f2f' : '#1e3d86';
  const inner = isRed ? '#f8d8d8' : '#d5e6ff';
  const ink = isRed ? '#a32634' : '#2249a6';
  const heroStroke = isRed ? '#ffd166' : '#9ae3ff';
  const heroMark = piece.hero
    ? `<circle cx="36" cy="36" r="30" fill="none" stroke="${heroStroke}" stroke-width="3" stroke-dasharray="3 3" />`
    : '';
  const label = escapeSvgText(pieceGlyph(piece.kind));
  const star = piece.hero ? `<text x="53" y="18" font-size="11" fill="${heroStroke}" font-family="Rajdhani, sans-serif">*</text>` : '';
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 72 72" role="img" aria-label="${escapeSvgText(pieceName(piece.kind))}">
<defs>
<radialGradient id="${gradId}" cx="30%" cy="24%" r="70%">
<stop offset="0%" stop-color="${inner}" />
<stop offset="100%" stop-color="${outer}" />
</radialGradient>
</defs>
<rect x="4" y="4" width="64" height="64" rx="16" fill="url(#${gradId})" />
${heroMark}
<rect x="13" y="14" width="46" height="44" rx="11" fill="rgba(255,255,255,0.8)" stroke="${ink}" stroke-width="2.2" />
<text x="36" y="42" text-anchor="middle" fill="${ink}" font-family="IBM Plex Mono, monospace" font-size="18" font-weight="700">${label}</text>
${star}
</svg>`;
  const encoded = `data:image/svg+xml;utf8,${encodeURIComponent(svg)}`;
  vectorSpriteCache[cacheKey] = encoded;
  return encoded;
}

function renderPieceToken(piece) {
  const token = document.createElement('div');
  token.className = `piece ${piece.player}${piece.hero ? ' hero' : ''} theme-${uiPrefs.pieceTheme}`;

  const sprite = spriteForPiece(piece);
  const wantsSprite = uiPrefs.pieceTheme === 'sprite';
  const wantsVector = uiPrefs.pieceTheme === 'vector';
  const useSprite = wantsSprite && !!sprite;

  if (useSprite || wantsVector || wantsSprite) {
    const img = document.createElement('img');
    img.className = `piece-img ${useSprite ? 'sprite' : 'vector'}`;
    img.src = useSprite ? sprite : vectorSpriteForPiece(piece);
    img.alt = `${piece.player} ${piece.kind}`;
    token.appendChild(img);
    return token;
  }

  const fb = document.createElement('div');
  fb.className = 'piece-fallback';
  fb.textContent = piece.hero ? `${pieceGlyph(piece.kind)}*` : pieceGlyph(piece.kind);
  token.appendChild(fb);
  return token;
}

function pieceName(kind) {
  const names = textMap('pieceNameByCode');
  return names[kind] || kind;
}

function terrainLabel(c, r) {
  if (isRiverRow(r)) return t('terrainRiver');
  if (isSea(c)) return t('terrainSea');
  return t('terrainLand');
}

function buildCellAriaLabel({ c, r, piece, isSelected, isLegalMove, isLegalCapture, isLastTarget, isLastOrigin }) {
  const occupancy = piece
    ? t('cellOccupied', { side: sideCaps(piece.player), piece: pieceName(piece.kind) })
    : t('cellEmpty');
  const flags = [];
  if (piece && piece.hero) flags.push(t('cellHeroFlag'));
  if (isSelected) flags.push(t('cellSelectedFlag'));
  if (isLegalCapture) flags.push(t('cellLegalCaptureFlag'));
  else if (isLegalMove) flags.push(t('cellLegalMoveFlag'));
  if (isLastTarget) flags.push(t('cellLastToFlag'));
  if (isLastOrigin) flags.push(t('cellLastFromFlag'));
  const suffix = flags.length ? `, ${flags.join(', ')}` : '';
  return t('cellLabel', {
    col: c,
    row: r,
    terrain: terrainLabel(c, r),
    occupancy,
    flags: suffix
  });
}

function tutorialStepDef() {
  return TUTORIAL_STEPS[tutorialStep] || null;
}

function tutorialBoardCoordFromView(viewCol, viewRow) {
  return { c: viewCol, r: (ROWS - 1 - viewRow) };
}

function tutorialIsCompleted() {
  try {
    return window.localStorage.getItem(TUTORIAL_STORAGE_KEY) === 'true';
  } catch (_) {
    return false;
  }
}

function setTutorialCompleted(done = true) {
  try {
    window.localStorage.setItem(TUTORIAL_STORAGE_KEY, done ? 'true' : 'false');
  } catch (_) {
    // ignore storage failures
  }
}

function quickTutorialPromptSeen() {
  try {
    return window.localStorage.getItem(QUICK_TUTORIAL_STORAGE_KEY) === 'true';
  } catch (_) {
    return false;
  }
}

function setQuickTutorialPromptSeen(done = true) {
  try {
    window.localStorage.setItem(QUICK_TUTORIAL_STORAGE_KEY, done ? 'true' : 'false');
  } catch (_) {
    // ignore storage failures
  }
}

function shouldShowQuickTutorialOverlay() {
  if (!quickTutorialOverlayEl) return false;
  if (!hasStartedGame || !state || state.game_over) return false;
  if (isOnlineMultiplayer()) return false;
  if (tutorialActive) return false;
  if (tutorialIsCompleted()) return false;
  if (quickTutorialPromptSeen()) return false;
  return true;
}

function updateQuickTutorialOverlay() {
  if (!quickTutorialOverlayEl) return;
  quickTutorialOverlayEl.hidden = !shouldShowQuickTutorialOverlay();
}

function resetTutorialState() {
  tutorialStep = 0;
  tutorialPieces = TUTORIAL_INITIAL_PIECES.map((piece) => ({ ...piece }));
  tutorialAllowedMoves = [];
  clearTutorialDragState();
}

function tutorialPieceById(pieceId) {
  return tutorialPieces.find((piece) => piece.id === pieceId) || null;
}

function tutorialVisiblePieces(step = tutorialStepDef()) {
  if (!tutorialIsMoveStep(step)) return [];
  const piece = tutorialPieceById(step.pieceId);
  return piece ? [piece] : [];
}

function tutorialIsMoveStep(step = tutorialStepDef()) {
  return !!(step && step.mode === 'move' && step.pieceId);
}

function tutorialCanStand(kind, c, r) {
  if (kind === 'Af') return true;
  if (kind === 'N') return isSea(c) || isRiverRow(r);
  return !isSea(c) && !isRiverRow(r);
}

function pushTutorialMove(moves, piece, c, r) {
  if (c < 0 || c >= COLS || r < 0 || r >= ROWS) return;
  if (!tutorialCanStand(piece.kind, c, r)) return;
  moves.push({ c, r });
}

function tutorialMovesForStep(step) {
  if (!tutorialIsMoveStep(step)) return [];
  const piece = tutorialPieceById(step.pieceId);
  if (!piece) return [];

  const moves = [];
  const ORTHO = [[1, 0], [-1, 0], [0, 1], [0, -1]];
  const DIAG = [[1, 1], [1, -1], [-1, 1], [-1, -1]];
  const HEROIC = step.id === 'hero' || !!piece.hero;
  const lineAdd = (dirs, maxDist) => {
    dirs.forEach(([dc, dr]) => {
      for (let dist = 1; dist <= maxDist; dist++) {
        pushTutorialMove(moves, piece, piece.col + dc * dist, piece.row + dr * dist);
      }
    });
  };

  switch (piece.kind) {
    case 'H':
      return moves;
    case 'C':
      lineAdd(ORTHO, 10);
      return moves;
    case 'In':
      lineAdd(ORTHO, 1);
      return moves;
    case 'M':
      lineAdd(ORTHO.concat(DIAG), 1);
      return moves;
    case 'T':
      lineAdd(ORTHO, HEROIC ? 3 : 2);
      if (HEROIC) lineAdd(DIAG, 1);
      return moves;
    case 'E':
      lineAdd(ORTHO, 1);
      return moves;
    case 'A':
      lineAdd(ORTHO, HEROIC ? 4 : 3);
      if (HEROIC) lineAdd(DIAG, 1);
      return moves;
    case 'Aa':
      lineAdd(ORTHO, 1);
      return moves;
    case 'Ms':
      lineAdd(ORTHO, 2);
      return moves;
    case 'Af':
      lineAdd(ORTHO.concat(DIAG), HEROIC ? 5 : 4);
      return moves;
    case 'N':
      lineAdd(ORTHO.concat(DIAG), 4);
      return moves;
    default:
      return moves;
  }

}

function tutorialMoveKey(c, r) {
  return `${c},${r}`;
}

function tutorialIsLegalTarget(c, r) {
  return tutorialAllowedMoves.some((m) => m.c === c && m.r === r);
}

function showExplanationText(text) {
  if (tutorialTextEl) tutorialTextEl.textContent = text || '';
}

function highlightCells(allowedMoves) {
  tutorialAllowedMoves = Array.isArray(allowedMoves) ? allowedMoves.slice() : [];
  renderTutorialBoard();
}

function renderTutorialBoard() {
  if (!tutorialBoardEl) return;
  tutorialBoardEl.innerHTML = '';

  const step = tutorialStepDef();
  const legalSet = new Set(tutorialAllowedMoves.map((m) => tutorialMoveKey(m.c, m.r)));
  const movePieceId = tutorialIsMoveStep(step) ? step.pieceId : '';
  const visiblePieces = tutorialVisiblePieces(step);
  const frag = document.createDocumentFragment();

  for (let viewRow = 0; viewRow < ROWS; viewRow++) {
    for (let viewCol = 0; viewCol < COLS; viewCol++) {
      const boardPos = tutorialBoardCoordFromView(viewCol, viewRow);
      const c = boardPos.c;
      const r = boardPos.r;
      const cell = document.createElement('button');
      cell.type = 'button';
      cell.className = `tutorial-cell cell ${terrainClass(c, r)}`;
      if (isReef(c, r)) cell.classList.add('reef');
      cell.dataset.col = String(c);
      cell.dataset.row = String(r);
      cell.dataset.viewCol = String(viewCol);
      cell.dataset.viewRow = String(viewRow);
      cell.setAttribute('aria-colindex', String(viewCol + 1));
      cell.setAttribute('aria-rowindex', String(viewRow + 1));

      const isLegal = legalSet.has(tutorialMoveKey(c, r));
      if (isLegal) cell.classList.add('tutorial-legal');

      if (tutorialFocusCell && tutorialFocusCell.c === c && tutorialFocusCell.r === r) {
        cell.classList.add('tutorial-drop-target');
      }

      const piece = visiblePieces.find((p) => p.col === c && p.row === r) || null;
      if (piece) {
        const token = renderPieceToken(piece);
        token.classList.add('tutorial-token');
        token.dataset.tutorialPieceId = piece.id;
        if (piece.id === movePieceId) {
          token.classList.add('tutorial-piece-focus');
          token.addEventListener('pointerdown', onTutorialPiecePointerDown);
        }
        cell.appendChild(token);
      }

      cell.addEventListener('click', onTutorialCellClick);
      frag.appendChild(cell);
    }
  }

  tutorialBoardEl.appendChild(frag);
}

function updateTutorialButtons() {
  const step = tutorialStepDef();
  if (!step) return;
  const isWelcome = step.mode === 'welcome';
  const atLast = tutorialStep >= TUTORIAL_STEPS.length - 1;

  if (tutorialPrevBtn) tutorialPrevBtn.disabled = tutorialStep <= 0;
  if (tutorialStartBtn) tutorialStartBtn.hidden = !isWelcome;
  if (tutorialNextBtn) {
    tutorialNextBtn.hidden = isWelcome;
    tutorialNextBtn.textContent = atLast ? 'FINISH' : 'NEXT';
    tutorialNextBtn.disabled = false;
  }
}

function renderTutorialStep() {
  if (!tutorialModalEl) return;
  const step = tutorialStepDef();
  if (!step) return;

  tutorialModalEl.dataset.stepType = step.mode;

  if (tutorialTitleEl) tutorialTitleEl.textContent = step.title;
  if (tutorialProgressEl) tutorialProgressEl.textContent = `Step ${tutorialStep + 1} / ${TUTORIAL_STEPS.length}`;
  showExplanationText(step.text);

  const allowedMoves = tutorialMovesForStep(step);
  highlightCells(allowedMoves);
  updateTutorialButtons();
}

function nextTutorial() {
  const step = tutorialStepDef();
  if (!step) return;

  if (tutorialStep >= TUTORIAL_STEPS.length - 1) {
    setTutorialCompleted(true);
    closeTutorial(false);
    return;
  }

  tutorialStep += 1;
  renderTutorialStep();
}

function prevTutorial() {
  if (tutorialStep <= 0) return;
  tutorialStep -= 1;
  renderTutorialStep();
}

function closeTutorial(markComplete = false) {
  if (!tutorialModalEl) return;
  if (markComplete) setTutorialCompleted(true);
  if (markComplete || tutorialIsCompleted()) setQuickTutorialPromptSeen(true);
  clearTutorialDragState();
  tutorialActive = false;
  tutorialModalEl.classList.remove('show');
  tutorialModalEl.setAttribute('aria-hidden', 'true');
  updateQuickTutorialOverlay();
  const afterClose = pendingTutorialCloseAction;
  pendingTutorialCloseAction = null;
  if (typeof afterClose === 'function') {
    window.setTimeout(() => {
      afterClose();
    }, 0);
  }
}

function openTutorial({ replay = false, auto = false } = {}) {
  if (!tutorialModalEl) return;
  if (auto && tutorialIsCompleted()) return;
  if (replay) resetTutorialState();

  tutorialActive = true;
  setQuickTutorialPromptSeen(true);
  tutorialModalEl.classList.add('show');
  tutorialModalEl.setAttribute('aria-hidden', 'false');
  renderTutorialStep();
  if (tutorialCardEl) tutorialCardEl.focus();
  updateQuickTutorialOverlay();
}

function tutorialCellFromPoint(x, y) {
  const el = document.elementFromPoint(x, y);
  if (!el) return null;
  return el.closest('.tutorial-cell');
}

function setTutorialFocusCellFromElement(cell) {
  if (!cell) {
    tutorialFocusCell = null;
    renderTutorialBoard();
    return;
  }
  const c = Number(cell.dataset.col);
  const r = Number(cell.dataset.row);
  if (!Number.isFinite(c) || !Number.isFinite(r)) return;
  if (tutorialFocusCell && tutorialFocusCell.c === c && tutorialFocusCell.r === r) return;
  tutorialFocusCell = { c, r };
  renderTutorialBoard();
}

function clearTutorialDragState() {
  if (tutorialDrag) {
    window.removeEventListener('pointermove', onTutorialPointerMove);
    window.removeEventListener('pointerup', onTutorialPointerUp);
    if (tutorialDrag.ghost && tutorialDrag.ghost.parentNode) {
      tutorialDrag.ghost.parentNode.removeChild(tutorialDrag.ghost);
    }
    tutorialDrag = null;
  }
  tutorialFocusCell = null;
  if (tutorialActive) renderTutorialBoard();
}

function positionTutorialGhost(ghost, clientX, clientY) {
  ghost.style.left = `${clientX}px`;
  ghost.style.top = `${clientY}px`;
}

function onTutorialPointerMove(ev) {
  if (!tutorialDrag || !tutorialDrag.ghost) return;
  positionTutorialGhost(tutorialDrag.ghost, ev.clientX, ev.clientY);

  const cell = tutorialCellFromPoint(ev.clientX, ev.clientY);
  if (!cell) {
    setTutorialFocusCellFromElement(null);
    return;
  }
  const c = Number(cell.dataset.col);
  const r = Number(cell.dataset.row);
  if (tutorialIsLegalTarget(c, r)) setTutorialFocusCellFromElement(cell);
  else setTutorialFocusCellFromElement(null);
}

function applyTutorialMove(pieceId, dc, dr) {
  const piece = tutorialPieceById(pieceId);
  if (!piece) return;
  piece.col = dc;
  piece.row = dr;
  playSound('move');
  renderTutorialStep();
}

function onTutorialPointerUp(ev) {
  if (!tutorialDrag) return;

  const pieceId = tutorialDrag.pieceId;
  const cell = tutorialCellFromPoint(ev.clientX, ev.clientY);
  clearTutorialDragState();

  if (!cell) return;
  const c = Number(cell.dataset.col);
  const r = Number(cell.dataset.row);
  if (!Number.isFinite(c) || !Number.isFinite(r)) return;
  if (!tutorialIsLegalTarget(c, r)) return;
  applyTutorialMove(pieceId, c, r);
}

function onTutorialPiecePointerDown(ev) {
  const step = tutorialStepDef();
  if (!tutorialIsMoveStep(step) || !tutorialActive) return;

  const pieceId = ev.currentTarget.dataset.tutorialPieceId;
  if (!pieceId || pieceId !== step.pieceId) return;

  ev.preventDefault();
  clearTutorialDragState();

  const ghost = ev.currentTarget.cloneNode(true);
  ghost.classList.add('tutorial-drag-ghost');
  ghost.removeAttribute('id');
  document.body.appendChild(ghost);
  positionTutorialGhost(ghost, ev.clientX, ev.clientY);

  tutorialDrag = { pieceId, ghost };
  window.addEventListener('pointermove', onTutorialPointerMove);
  window.addEventListener('pointerup', onTutorialPointerUp);
}

function onTutorialCellClick(ev) {
  const step = tutorialStepDef();
  if (!tutorialIsMoveStep(step)) return;
  const c = Number(ev.currentTarget.dataset.col);
  const r = Number(ev.currentTarget.dataset.row);
  if (!Number.isFinite(c) || !Number.isFinite(r)) return;
  if (!tutorialIsLegalTarget(c, r)) return;
  applyTutorialMove(step.pieceId, c, r);
}

function maybeAutoOpenTutorial() {
  if (tutorialIsCompleted()) return;
  openTutorial({ replay: true, auto: true });
}

function onCellKeyDown(ev) {
  const moves = {
    ArrowUp: { dc: 0, dr: -1 },
    ArrowDown: { dc: 0, dr: 1 },
    ArrowLeft: { dc: -1, dr: 0 },
    ArrowRight: { dc: 1, dr: 0 }
  };
  const move = moves[ev.key];
  if (!move) return;

  const viewCol = Number(ev.currentTarget.dataset.viewCol);
  const viewRow = Number(ev.currentTarget.dataset.viewRow);
  if (!Number.isFinite(viewCol) || !Number.isFinite(viewRow)) return;

  ev.preventDefault();
  const nextCol = Math.max(0, Math.min(COLS - 1, viewCol + move.dc));
  const nextRow = Math.max(0, Math.min(ROWS - 1, viewRow + move.dr));
  const target = boardEl.querySelector(`.cell[data-view-col="${nextCol}"][data-view-row="${nextRow}"]`);
  if (target) target.focus();
}

function drawBoard() {
  boardEl.innerHTML = '';
  const drawState = currentViewState();
  if (!drawState) return;

  const reviewing = reviewIndex >= 0;
  const hintsEnabled = uiPrefs.moveHints && !reviewing;
  const selectedPiece = reviewing ? null : pieceById(selectedPid, drawState);
  const legalMoves = selectedPiece ? legalMovesForPid(selectedPid, drawState) : [];
  const legalSet = new Map(legalMoves.map(m => [key(m.dc, m.dr), m]));
  const markerFrom = reviewing
    ? (reviewIndex > 0 ? moveHistory[reviewIndex - 1]?.from || null : null)
    : lastMoveFrom;
  const fx = (!reviewing && uiPrefs.animationsEnabled) ? boardFx : null;
  const fxToKey = fx && fx.to ? key(fx.to.c, fx.to.r) : '';
  const fxFromKey = fx && fx.from ? key(fx.from.c, fx.from.r) : '';

  const frag = document.createDocumentFragment();

  for (let viewRow = 0; viewRow < ROWS; viewRow++) {
    for (let viewCol = 0; viewCol < COLS; viewCol++) {
      const boardPos = boardCoordFromView(viewCol, viewRow);
      const c = boardPos.c;
      const r = boardPos.r;
      const cell = document.createElement('button');
      cell.type = 'button';
      cell.className = `cell ${terrainClass(c, r)}`;
      if (isReef(c, r)) cell.classList.add('reef');
      cell.dataset.col = String(c);
      cell.dataset.row = String(r);
      cell.dataset.viewCol = String(viewCol);
      cell.dataset.viewRow = String(viewRow);
      cell.setAttribute('role', 'gridcell');
      cell.setAttribute('aria-colindex', String(viewCol + 1));
      cell.setAttribute('aria-rowindex', String(viewRow + 1));

      const piece = pieceAt(c, r, drawState);
      const legal = legalSet.has(key(c, r));
      const showLegal = hintsEnabled && legal;
      let legalCapture = false;

      if (piece && piece.id === selectedPid) {
        cell.classList.add('selected');
      }

      if (legal) {
        const target = pieceAt(c, r, drawState);
        if (target && selectedPiece && target.player !== selectedPiece.player) {
          legalCapture = true;
          if (showLegal) cell.classList.add('legal-capture');
        } else if (showLegal) {
          cell.classList.add('legal-move');
        }
      }

      const isLastTarget = drawState.has_last_move && drawState.last_move.dc === c && drawState.last_move.dr === r;
      const isLastOrigin = !!(markerFrom && markerFrom.c === c && markerFrom.r === r);

      if (isLastTarget) {
        cell.classList.add('last-target');
        cell.classList.add(drawState.last_move_player === 'red' ? 'last-red' : 'last-blue');
      }
      if (isLastOrigin) {
        cell.classList.add('last-origin');
        cell.classList.add(drawState.last_move_player === 'red' ? 'last-red' : 'last-blue');
      }

      const coordKey = key(c, r);
      const isFxTo = coordKey === fxToKey;
      const isFxFrom = coordKey === fxFromKey;
      if (isFxTo) {
        cell.classList.add('fx-to');
        cell.classList.add(fx && fx.player === 'blue' ? 'fx-blue' : 'fx-red');
        if (fx && fx.capture) cell.classList.add('fx-impact');
      }
      if (isFxFrom) {
        cell.classList.add('fx-from');
      }

      if (piece) {
        const token = renderPieceToken(piece);
        if (isFxTo) token.classList.add('piece-landed');
        if (piece.id === selectedPid) token.classList.add('piece-active');
        cell.appendChild(token);
      }

      const selected = piece && piece.id === selectedPid;
      cell.setAttribute('aria-selected', selected ? 'true' : 'false');
      cell.setAttribute('aria-label', buildCellAriaLabel({
        c,
        r,
        piece,
        isSelected: !!selected,
        isLegalMove: legal,
        isLegalCapture: legalCapture,
        isLastTarget,
        isLastOrigin
      }));
      cell.addEventListener('click', onCellClick);
      cell.addEventListener('keydown', onCellKeyDown);
      frag.appendChild(cell);
    }
  }

  boardEl.appendChild(frag);
}

function setStatusTheme() {
  statusBarEl.classList.remove('status-idle', 'status-red', 'status-blue', 'status-over', 'status-thinking');

  if (!state) {
    statusBarEl.classList.add('status-idle');
    statusTagEl.textContent = t('statusIdle');
    return;
  }

  if (reviewIndex >= 0) {
    statusBarEl.classList.add('status-idle');
    statusTagEl.textContent = t('statusReview');
    return;
  }

  if (state.game_over) {
    statusBarEl.classList.add('status-over');
    statusTagEl.textContent = t('statusGameOver');
    return;
  }

  if (state.turn === 'red') statusBarEl.classList.add('status-red');
  else statusBarEl.classList.add('status-blue');

  if (isOnlineMultiplayer() && guestMovePending) {
    statusBarEl.classList.add('status-thinking');
    statusTagEl.textContent = t('statusThinkingTag');
  } else if (!isLocalMultiplayer() && !isOnlineMultiplayer() && (botThinking || state.turn !== humanSide())) {
    statusBarEl.classList.add('status-thinking');
    statusTagEl.textContent = t('statusThinkingTag');
  } else {
    statusTagEl.textContent = t('statusToMoveTag', { side: sideCaps(state.turn) });
  }
}

function updateStatus() {
  if (!state) {
    const sideLabel = isLocalMultiplayer()
      ? t('bothSides')
      : (isOnlineMultiplayer() ? sideCaps(onlineRoleSide()) : sideCaps(selectedSide));
    statusEl.textContent = t('statusPressStart');
    metaEl.textContent = t('metaNoGame', {
      mode: modeLabel(selectedMode),
      playerMode: playerModeLabel(selectedPlayerMode, true),
      difficulty: difficultyLabel(selectedDifficulty),
      side: sideLabel
    });
    statYouEl.textContent = sideLabel;
    if (statDifficultyEl) statDifficultyEl.textContent = difficultyLabel(selectedDifficulty);
    statTurnEl.textContent = '-';
    statLegalEl.textContent = '-';
    statPiecesEl.textContent = '-';
    statSelectionEl.textContent = t('noSelection');
    setStatusTheme();
    updateBotButtonState();
    announceStatusLive();
    updateQuickTutorialOverlay();
    closePostGameModal();
    return;
  }

  const viewState = currentViewState();
  const reviewing = reviewIndex >= 0;
  const you = humanSide();
  const difficulty = activeDifficulty();
  const activePieces = viewState.pieces.filter(p => p.carrier_id < 0);
  const selected = reviewing ? null : pieceById(selectedPid, viewState);

  if (reviewing) {
    if (reviewIndex === 0) statusEl.textContent = t('reviewingInitial');
    else statusEl.textContent = t('reviewingMove', { index: reviewIndex, total: moveHistory.length });
  } else if (state.game_over) {
    statusEl.textContent = t('gameOver', { result: state.result });
  } else if (isOnlineMultiplayer()) {
    if (guestMovePending) {
      statusEl.textContent = t('onlineStatusMovePending');
    } else {
      statusEl.textContent = t('onlineTurn', { side: sideCaps(state.turn) });
    }
  } else if (isLocalMultiplayer()) {
    statusEl.textContent = t('localTurn', { side: sideCaps(state.turn) });
  } else if (botThinking || state.turn !== you) {
    statusEl.textContent = t('cpuThinking', { side: sideCaps(state.turn) });
  } else {
    statusEl.textContent = t('yourTurn', { side: sideCaps(you) });
  }

  let lastMoveTxt = t('lastMoveNone');
  const markerFrom = reviewing
    ? (reviewIndex > 0 ? moveHistory[reviewIndex - 1]?.from || null : null)
    : lastMoveFrom;
  if (viewState.has_last_move) {
    if (markerFrom) {
      lastMoveTxt =
        `${sideCaps(viewState.last_move_player)} (${markerFrom.c},${markerFrom.r}) -> ` +
        `(${viewState.last_move.dc},${viewState.last_move.dr})`;
    } else {
      lastMoveTxt = `${sideCaps(viewState.last_move_player)} -> (${viewState.last_move.dc},${viewState.last_move.dr})`;
    }
    if (viewState.last_move_capture) lastMoveTxt += t('captureSuffix');
  }

  const reviewMeta = reviewing ? t('reviewMeta') : '';
  metaEl.textContent = t('metaWithGame', {
    mode: modeLabel(activeMode()),
    playerMode: playerModeLabel(selectedPlayerMode, true),
    difficulty: difficultyLabel(difficulty),
    game: gameId ? gameId.slice(0, 8) : '--',
    lastMove: lastMoveTxt,
    reviewMeta
  });

  statYouEl.textContent = isLocalMultiplayer() ? t('bothSides') : sideCaps(you);
  if (statDifficultyEl) statDifficultyEl.textContent = difficultyLabel(difficulty);
  statTurnEl.textContent = reviewing ? `${sideCaps(viewState.turn)}*` : sideCaps(state.turn);
  statLegalEl.textContent = reviewing ? '-' : String(state.legal_moves.length);
  statPiecesEl.textContent = String(activePieces.length);

  if (selected) {
    statSelectionEl.textContent = t('selectionFormat', {
      player: sideCaps(selected.player),
      kind: selected.kind,
      hero: selected.hero ? '*' : '',
      col: selected.col,
      row: selected.row
    });
  } else {
    statSelectionEl.textContent = t('noSelection');
  }

  setStatusTheme();
  updateBotButtonState();
  announceStatusLive();
  updateQuickTutorialOverlay();
  maybeShowPostGameModal();
}

function otherSide(side) {
  return side === 'red' ? 'blue' : 'red';
}

function winnerFromResultText(resultText) {
  const text = String(resultText || '').toLowerCase();
  if (text.includes('red wins') || text.includes('đỏ thắng')) return 'red';
  if (text.includes('blue wins') || text.includes('xanh thắng')) return 'blue';
  return '';
}

function pieceStatsLabel(kind) {
  return pieceName(kind) || pieceGlyph(kind);
}

function computeDestroyedByAttacker() {
  const totals = { red: Object.create(null), blue: Object.create(null) };
  if (!Array.isArray(stateHistory) || stateHistory.length < 2) return totals;

  for (let i = 1; i < stateHistory.length; i++) {
    const prev = stateHistory[i - 1];
    const next = stateHistory[i];
    if (!prev || !next || !next.last_move_player) continue;
    const attacker = next.last_move_player;
    if (attacker !== 'red' && attacker !== 'blue') continue;
    const nextIds = new Set(next.pieces.map((p) => p.id));
    for (const p of prev.pieces) {
      if (nextIds.has(p.id)) continue;
      if (p.kind === 'H') continue;
      if (p.player === attacker) continue;
      totals[attacker][p.kind] = (totals[attacker][p.kind] || 0) + 1;
    }
  }

  return totals;
}

function formatDestroyedKindsLine(kindsMap) {
  const entries = Object.entries(kindsMap || {}).sort((a, b) => {
    const va = Number(a[1]) || 0;
    const vb = Number(b[1]) || 0;
    if (vb !== va) return vb - va;
    return a[0].localeCompare(b[0]);
  });
  if (!entries.length) return selectedLanguage === 'vi' ? 'Không có' : 'None';
  return entries.map(([kind, count]) => `${pieceStatsLabel(kind)} x${count}`).join('  ·  ');
}

function clearPostGameShareStatus() {
  if (!postGameShareStatusEl) return;
  postGameShareStatusEl.textContent = '';
  postGameShareStatusEl.classList.remove('ok');
}

function setPostGameShareStatus(message, ok = false) {
  if (!postGameShareStatusEl) return;
  postGameShareStatusEl.textContent = String(message || '');
  postGameShareStatusEl.classList.toggle('ok', !!ok);
}

function closePostGameModal() {
  if (!postGameModalEl) return;
  postGameModalEl.classList.remove('show');
  postGameModalEl.setAttribute('aria-hidden', 'true');
  clearPostGameShareStatus();
}

function showPostGameModal() {
  if (!postGameModalEl || !postGameCardEl || !state || !state.game_over) return;

  const winner = winnerFromResultText(state.result);
  const you = humanSide();
  let title = selectedLanguage === 'vi' ? 'Trận đấu kết thúc' : 'Battle Complete';
  let tone = 'draw';
  if (winner) {
    if (!isLocalMultiplayer() && winner === you) {
      title = selectedLanguage === 'vi' ? 'Chiến thắng' : 'Victory';
      tone = 'victory';
    } else if (!isLocalMultiplayer() && winner !== you) {
      title = selectedLanguage === 'vi' ? 'Thất bại' : 'Defeat';
      tone = 'defeat';
    } else {
      title = `${sideCaps(winner)} ${selectedLanguage === 'vi' ? 'chiến thắng' : 'Wins'}`;
      tone = winner === 'red' ? 'victory' : 'defeat';
    }
  } else {
    title = selectedLanguage === 'vi' ? 'Hòa' : 'Draw';
  }

  if (postGameTitleEl) postGameTitleEl.textContent = title;
  if (postGameResultEl) postGameResultEl.textContent = state.result || (selectedLanguage === 'vi' ? 'Trận đấu đã kết thúc.' : 'Game over.');
  postGameCardEl.classList.remove('victory', 'defeat', 'draw');
  postGameCardEl.classList.add(tone);

  const totals = computeDestroyedByAttacker();
  if (postGameStatsEl) {
    const redLine = formatDestroyedKindsLine(totals.red);
    const blueLine = formatDestroyedKindsLine(totals.blue);
    postGameStatsEl.innerHTML =
      `<div class="postgame-stats-row"><div class="postgame-stats-side">${sideCaps('red')}</div><div class="postgame-stats-line">${redLine}</div></div>` +
      `<div class="postgame-stats-row"><div class="postgame-stats-side">${sideCaps('blue')}</div><div class="postgame-stats-line">${blueLine}</div></div>`;
  }

  clearPostGameShareStatus();
  postGameModalEl.classList.add('show');
  postGameModalEl.setAttribute('aria-hidden', 'false');
  postGameCardEl.focus();
}

function maybeShowPostGameModal() {
  if (!state || !state.game_over || !postGameModalEl) {
    closePostGameModal();
    return;
  }
  const signature = `${stateHash(state)}:${moveHistory.length}`;
  if (postGameShownSignature === signature) return;
  postGameShownSignature = signature;
  showPostGameModal();
}

function encodeReplayPayload(payload) {
  const json = JSON.stringify(payload);
  const enc = new TextEncoder().encode(json);
  let binary = '';
  const CHUNK = 0x8000;
  for (let i = 0; i < enc.length; i += CHUNK) {
    binary += String.fromCharCode(...enc.subarray(i, i + CHUNK));
  }
  return btoa(binary);
}

function decodeReplayPayload(token) {
  const clean = String(token || '').trim();
  if (!clean) return null;
  const binary = atob(clean);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
  const json = new TextDecoder().decode(bytes);
  const parsed = JSON.parse(json);
  return parsed && typeof parsed === 'object' ? parsed : null;
}

function buildReplayPayload() {
  const moves = moveHistory
    .map((entry) => {
      if (!entry || !entry.to) return null;
      return {
        pid: Number(entry.pid),
        dc: Number(entry.to.c),
        dr: Number(entry.to.r)
      };
    })
    .filter((m) => Number.isFinite(m.pid) && Number.isFinite(m.dc) && Number.isFinite(m.dr))
    .slice(0, REPLAY_MAX_MOVES);

  return {
    v: 1,
    mode: activeMode(),
    difficulty: activeDifficulty(),
    side: selectedSide,
    playerMode: selectedPlayerMode,
    moves
  };
}

function buildReplayLink() {
  const payload = buildReplayPayload();
  const token = encodeReplayPayload(payload);
  const url = new URL(window.location.href);
  url.searchParams.delete('match');
  url.searchParams.delete('replay');
  url.hash = `replay=${encodeURIComponent(token)}`;
  return url.toString();
}

function replayTokenFromUrl() {
  const url = new URL(window.location.href);
  const queryToken = url.searchParams.get('replay');
  if (queryToken) return queryToken;
  if (url.hash && url.hash.startsWith('#replay=')) {
    return decodeURIComponent(url.hash.slice('#replay='.length));
  }
  return '';
}

async function loadReplayFromPayload(payload) {
  if (!payload || typeof payload !== 'object') throw new Error('Invalid replay payload.');
  const mode = isValidMode(payload.mode) ? payload.mode : 'full';
  const difficulty = isValidDifficulty(payload.difficulty) ? payload.difficulty : 'medium';
  const side = payload.side === 'blue' ? 'blue' : 'red';
  const rawMoves = Array.isArray(payload.moves) ? payload.moves : [];

  await newGame(mode, side, difficulty, 'local');
  const api = await ensureGameApi();

  let currentState = state;
  const nextStateHistory = [cloneState(currentState)];
  const nextMoveHistory = [];

  for (const mvRaw of rawMoves.slice(0, REPLAY_MAX_MOVES)) {
    const move = normalizeMove(mvRaw);
    if (!move || !moveIsLegal(move, currentState)) break;
    const prev = cloneState(currentState);
    const data = await api.move({ game_id: gameId, move });
    currentState = data.state;
    const fromSquare = findLastMoveFrom(prev, currentState);
    nextMoveHistory.push({
      player: currentState.last_move_player,
      from: fromSquare || null,
      to: { c: currentState.last_move.dc, r: currentState.last_move.dr },
      capture: !!currentState.last_move_capture,
      pid: currentState.last_move.pid
    });
    nextStateHistory.push(cloneState(currentState));
    if (currentState.game_over) break;
  }

  state = currentState;
  moveHistory = nextMoveHistory;
  stateHistory = nextStateHistory;
  selectedPid = null;
  reviewIndex = stateHistory.length > 1 ? stateHistory.length - 1 : -1;
  lastMoveFrom = moveHistory.length > 0 ? moveHistory[moveHistory.length - 1].from || null : null;
  hasStartedGame = true;
  postGameShownSignature = '';
  clearBoardFx();
  closeSetupMenu();
  updateSetupSelectionUI();
  updateQuickRestartVisibility();
  updateHistoryUI();
  updateStatus();
  drawBoard();
}

async function tryLoadReplayFromUrl() {
  const token = replayTokenFromUrl();
  if (!token || replayLoading) return false;
  replayLoading = true;
  try {
    const payload = decodeReplayPayload(token);
    if (!payload) return false;
    await loadReplayFromPayload(payload);
    return true;
  } catch (err) {
    console.warn('[Commander Chess] replay load failed', err);
    return false;
  } finally {
    replayLoading = false;
  }
}

function updatePwaInstallButton() {
  if (!pwaInstallBtn) return;
  const standalone = (window.matchMedia && window.matchMedia('(display-mode: standalone)').matches)
    || !!window.navigator.standalone;
  const canPrompt = !!deferredInstallPrompt && !standalone;
  pwaInstallBtn.hidden = !canPrompt;
  pwaInstallBtn.disabled = !canPrompt;
}

function showError(error, retryAction = null) {
  playSound('invalid');
  const rawMsg = errorMessage(error);
  const authMsg = authFriendlyErrorMessage(rawMsg);
  const displayMsg = authMsg || (isEngineConnectionErrorMessage(rawMsg)
    ? t('engineUnavailable')
    : (rawMsg || t('requestFailed')));
  statusEl.textContent = t('errorPrefix', { msg: displayMsg });
  metaEl.textContent = retryAction ? t('retryHint') : t('requestFailed');
  setRetryAction(retryAction);
  announceStatusLive();
}

function stableJsonString(value) {
  if (value == null) return 'null';
  if (typeof value === 'number' || typeof value === 'boolean') return JSON.stringify(value);
  if (typeof value === 'string') return JSON.stringify(value);
  if (Array.isArray(value)) {
    return `[${value.map((v) => stableJsonString(v)).join(',')}]`;
  }
  const keys = Object.keys(value).sort();
  const parts = keys.map((k) => `${JSON.stringify(k)}:${stableJsonString(value[k])}`);
  return `{${parts.join(',')}}`;
}

function hashFNV1a(text) {
  let hash = 0x811c9dc5;
  for (let i = 0; i < text.length; i++) {
    hash ^= text.charCodeAt(i);
    hash = Math.imul(hash, 0x01000193);
  }
  return (hash >>> 0).toString(16).padStart(8, '0');
}

function stateHash(srcState = state) {
  if (!srcState) return '';
  return hashFNV1a(stableJsonString(srcState));
}

function normalizeMove(move) {
  if (!move || typeof move !== 'object') return null;
  const pid = Number(move.pid);
  const dc = Number(move.dc);
  const dr = Number(move.dr);
  if (!Number.isInteger(pid) || !Number.isInteger(dc) || !Number.isInteger(dr)) return null;
  return { pid, dc, dr };
}

function moveMatches(a, b) {
  if (!a || !b) return false;
  return Number(a.pid) === Number(b.pid) && Number(a.dc) === Number(b.dc) && Number(a.dr) === Number(b.dr);
}

function moveIsLegal(move, srcState = state) {
  if (!srcState) return false;
  return srcState.legal_moves.some((m) => moveMatches(m, move));
}

async function fetchOnlineMatchDoc(matchId) {
  const fc = await ensureOnlineReady();
  const matchRef = fc.doc(fc.db, 'matches', matchId);
  const snap = await fc.getDoc(matchRef);
  if (!snap.exists()) return null;
  return { ref: matchRef, data: snap.data() };
}

async function waitForOnlineGuestJoin(matchId, timeoutMs = 12000) {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    const details = await fetchOnlineMatchDoc(matchId);
    if (details && details.data && details.data.guestUid) return details;
    await new Promise((resolve) => setTimeout(resolve, 500));
  }
  return fetchOnlineMatchDoc(matchId);
}

async function fetchOnlineMoves(matchId) {
  const fc = await ensureOnlineReady();
  const movesRef = fc.collection(fc.db, 'matches', matchId, 'moves');
  const q = fc.query(movesRef, fc.orderBy('ply'), fc.limit(2048));
  const snap = await fc.getDocs(q);
  return snap.docs.map((docSnap) => ({ id: docSnap.id, ...docSnap.data() }));
}

async function ensureOnlineRtcConnection(waitForReady = false) {
  if (!onlineSession.matchId || !onlineSession.role) return false;
  if (!onlineRtc) {
    const createTransport = await ensureRtcFactory();
    onlineRtc = createTransport({
      matchId: onlineSession.matchId,
      role: onlineSession.role,
      onMessage: (msg) => {
        handleOnlineSignalMessage(msg).catch((err) => {
          console.warn('[Commander Chess] online signal error', err);
        });
      },
      onState: ({ state: rtcState }) => {
        if (rtcState === 'connected') {
          setOnlineStatus('onlineStatusConnected', null, true);
        } else if (rtcState === 'connecting' || rtcState === 'signaling') {
          setOnlineStatus('onlineStatusConnecting', null, true);
        } else if (rtcState === 'disconnected' || rtcState === 'failed') {
          setOnlineStatus('onlineStatusDisconnected', null, true);
        }
        updateOnlineStatusUI();
      }
    });
    await onlineRtc.connect();
  }
  if (!waitForReady) return true;
  const ok = await onlineRtc.waitUntilReady(18000);
  return ok;
}

function onlineStartBaseConfig(matchData = null) {
  const mode = (matchData && isValidMode(matchData.gameMode)) ? matchData.gameMode : selectedMode;
  const difficulty = (matchData && isValidDifficulty(matchData.difficulty))
    ? matchData.difficulty
    : selectedDifficulty;
  return { mode, difficulty };
}

async function restoreOnlineStateFromFirestore(matchData = null) {
  if (!onlineSession.matchId) throw new Error(t('onlineStatusNoSession'));

  const baseConfig = onlineStartBaseConfig(matchData);
  selectedMode = baseConfig.mode;
  selectedDifficulty = baseConfig.difficulty;
  selectedSide = onlineRoleSide();
  if (sideSelect) sideSelect.value = selectedSide;
  if (difficultySelect) difficultySelect.value = selectedDifficulty;

  const api = await ensureGameApi();
  const base = await api.newGame({
    human_player: 'red',
    game_mode: selectedMode,
    difficulty: selectedDifficulty
  });

  let currentState = base.state;
  const nextStateHistory = [cloneState(currentState)];
  const nextMoveHistory = [];
  const moves = await fetchOnlineMoves(onlineSession.matchId);

  for (const entry of moves) {
    const move = normalizeMove(entry.move);
    if (!move) continue;
    const prev = cloneState(currentState);
    const data = await api.move({ game_id: base.game_id, move });
    currentState = data.state;
    const fromSquare = findLastMoveFrom(prev, currentState);
    nextMoveHistory.push({
      player: currentState.last_move_player,
      from: fromSquare || null,
      to: { c: currentState.last_move.dc, r: currentState.last_move.dr },
      capture: !!currentState.last_move_capture,
      pid: currentState.last_move.pid
    });
    nextStateHistory.push(cloneState(currentState));
  }

  gameId = base.game_id || `online-${onlineSession.matchId}`;
  state = currentState;
  moveHistory = nextMoveHistory;
  stateHistory = nextStateHistory;
  reviewIndex = -1;
  selectedPid = null;
  guestMovePending = false;
  hasStartedGame = true;
  lastMoveFrom = moveHistory.length > 0 ? moveHistory[moveHistory.length - 1].from || null : null;
  clearBoardFx();

  updateSetupSelectionUI();
  updateQuickRestartVisibility();
  updateHistoryUI();
  updateStatus();
  drawBoard();
}

async function applyStateSyncFromPeer(payload) {
  if (!payload || typeof payload.stateJson === 'undefined') return;
  const rawState = (typeof payload.stateJson === 'string')
    ? JSON.parse(payload.stateJson)
    : payload.stateJson;
  const api = await ensureGameApi();
  const synced = await api.setPosition(rawState);
  if (!gameId) gameId = `online-${onlineSession.matchId || 'peer'}`;
  state = synced;
  selectedSide = onlineRoleSide();
  if (sideSelect) sideSelect.value = selectedSide;
  selectedPid = null;
  lastMoveFrom = null;
  reviewIndex = -1;
  moveHistory = [];
  stateHistory = [cloneState(state)];
  guestMovePending = false;
  hasStartedGame = true;
  clearBoardFx();
  updateHistoryUI();
  updateStatus();
  drawBoard();
}

async function writeOnlineMoveLog(ply, move, by) {
  if (onlineSession.role !== 'host') return;
  if (!onlineSession.matchId) return;
  const fc = await ensureOnlineReady();
  await fc.addDoc(fc.collection(fc.db, 'matches', onlineSession.matchId, 'moves'), {
    ply,
    move,
    by,
    stateHash: stateHash(state),
    ts: fc.serverTimestamp()
  });
}

async function writeOnlineMatchStateUpdate({ move = null, ply = null } = {}) {
  if (!onlineSession.matchId) return;
  const fc = await ensureOnlineReady();
  const matchRef = fc.doc(fc.db, 'matches', onlineSession.matchId);
  const payload = {
    sideToMove: state ? state.turn : 'red',
    turn: state ? state.turn : 'red',
    updatedAt: fc.serverTimestamp()
  };
  if (move) payload.lastMove = move;
  if (Number.isInteger(ply)) payload.lastPly = ply;
  if (state && state.game_over) {
    payload.status = 'ended';
    payload.result = state.result || '';
  } else {
    payload.status = 'active';
  }
  await fc.updateDoc(matchRef, payload);
}

function sendOnlineMessage(payload, requireReady = false) {
  if (!onlineRtc) {
    if (requireReady) throw new Error(t('onlineStatusConnecting'));
    return false;
  }
  const sent = onlineRtc.send(payload);
  if (!sent && requireReady) throw new Error(t('onlineStatusConnecting'));
  return sent;
}

async function sendOnlineSyncState(reason = 'manual') {
  if (onlineSession.role !== 'host' || !state) return;
  sendOnlineMessage({
    t: 'sync_state',
    reason,
    ply: moveHistory.length,
    stateHash: stateHash(state),
    gameMode: activeMode(),
    difficulty: activeDifficulty(),
    stateJson: JSON.stringify(state)
  }, false);
}

async function applyOnlineAuthoritativeMove(move, by) {
  const ply = moveHistory.length;
  await sendMove(move.pid, move.dc, move.dr, { skipAutoBot: true });
  guestMovePending = false;

  await writeOnlineMoveLog(ply, move, by);
  await writeOnlineMatchStateUpdate({ move, ply });

  sendOnlineMessage({
    t: 'move',
    authoritative: true,
    by,
    ply,
    move,
    stateHash: stateHash(state)
  }, false);
}

async function handleOnlineSignalMessage(message) {
  if (!message || typeof message !== 'object') return;
  if (!isOnlineMultiplayer()) return;

  if (message.t === 'sync_request') {
    if (onlineSession.role === 'host') {
      await sendOnlineSyncState('peer_request');
    }
    return;
  }

  if (message.t === 'sync_state') {
    if (onlineSession.role === 'host') return;
    await applyStateSyncFromPeer(message);
    setOnlineStatus('onlineStatusConnected', null, true);
    updateOnlineStatusUI();
    return;
  }

  if (message.t !== 'move') return;
  const move = normalizeMove(message.move);
  if (!move || !state || state.game_over) return;

  const expectedPly = moveHistory.length;
  const incomingPly = Number(message.ply);
  if (!Number.isInteger(incomingPly) || incomingPly !== expectedPly) {
    if (onlineSession.role === 'host') {
      await sendOnlineSyncState('ply_mismatch');
    } else {
      sendOnlineMessage({ t: 'sync_request', reason: 'ply_mismatch' }, false);
    }
    return;
  }

  if (onlineSession.role === 'host') {
    if (message.authoritative) return;
    if (state.turn !== 'blue') {
      await sendOnlineSyncState('turn_mismatch');
      return;
    }
    if (message.stateHash && message.stateHash !== stateHash(state)) {
      await sendOnlineSyncState('state_hash_mismatch');
      return;
    }
    if (!moveIsLegal(move, state)) {
      await sendOnlineSyncState('illegal_move');
      return;
    }
    await applyOnlineAuthoritativeMove(move, 'guest');
    return;
  }

  if (!message.authoritative) return;
  if (!moveIsLegal(move, state)) {
    sendOnlineMessage({ t: 'sync_request', reason: 'illegal_move' }, false);
    return;
  }

  await sendMove(move.pid, move.dc, move.dr, { skipAutoBot: true });
  guestMovePending = false;
  if (message.stateHash && message.stateHash !== stateHash(state)) {
    sendOnlineMessage({ t: 'sync_request', reason: 'state_hash_mismatch' }, false);
  }
}

async function subscribeToOnlineMatch(matchId) {
  const fc = await ensureOnlineReady();
  clearOnlineMatchSubscription();
  const matchRef = fc.doc(fc.db, 'matches', matchId);
  onlineMatchUnsub = fc.onSnapshot(matchRef, (snap) => {
    if (!snap.exists()) {
      setOnlineStatus('onlineMatchNotFound', null, true);
      updateOnlineStatusUI();
      return;
    }
    const data = snap.data() || {};
    onlineSession.matchId = matchId;
    onlineSession.status = data.status || 'waiting';
    onlineSession.hostUid = data.hostUid || '';
    onlineSession.guestUid = data.guestUid || '';
    onlineSession.link = buildOnlineMatchLink(matchId);

    if (!onlineSession.role && data.hostUid && firebaseClient?.auth?.currentUser?.uid === data.hostUid) {
      onlineSession.role = 'host';
    } else if (!onlineSession.role && data.guestUid && firebaseClient?.auth?.currentUser?.uid === data.guestUid) {
      onlineSession.role = 'guest';
    }

    if (onlineSession.status === 'waiting') {
      setOnlineStatus('onlineStatusWaitingGuest', { code: matchId }, true);
    } else if (onlineSession.status === 'active') {
      setOnlineStatus('onlineStatusActive', null, true);
      ensureOnlineRtcConnection(false).catch((err) => {
        console.warn('[Commander Chess] RTC connect failed', err);
      });
    } else if (onlineSession.status === 'ended') {
      if (data.result) setOnlineStatus('gameOver', { result: data.result }, true);
    }
    updateOnlineStatusUI();
  }, (err) => {
    console.warn('[Commander Chess] match subscription failed', err);
    setOnlineStatus('requestFailed', null, true);
    updateOnlineStatusUI();
  });
}

async function createOnlineMatch() {
  const fc = await ensureOnlineReady();
  setOnlineStatus('onlineStatusCreating', null, true);
  updateOnlineStatusUI();

  const user = fc.auth.currentUser;
  const displayName = user?.displayName || `Guest-${(user?.uid || '').slice(0, 6)}`;
  const gameMode = selectedMode;
  const difficulty = selectedDifficulty;

  const docRef = await fc.addDoc(fc.collection(fc.db, 'matches'), {
    createdAt: fc.serverTimestamp(),
    status: 'waiting',
    hostUid: user?.uid || null,
    guestUid: null,
    hostName: displayName,
    guestName: '',
    sideToMove: 'red',
    turn: 'red',
    lastMove: null,
    lastPly: -1,
    result: '',
    gameMode,
    difficulty
  });

  clearOnlineSession(true);
  onlineSession.matchId = docRef.id;
  onlineSession.role = 'host';
  onlineSession.hostUid = user?.uid || '';
  onlineSession.status = 'waiting';
  onlineSession.link = buildOnlineMatchLink(docRef.id);
  if (onlineMatchCodeEl) onlineMatchCodeEl.value = docRef.id;
  if (joinMatchInputEl) joinMatchInputEl.value = docRef.id;
  updateStatus();
  setOnlineStatus('onlineStatusCreated', { code: docRef.id }, true);
  await subscribeToOnlineMatch(docRef.id);
  updateOnlineStatusUI();
}

async function joinOnlineMatch(matchCode) {
  const normalized = sanitizeMatchId(matchCode);
  if (!normalized) throw new Error(t('onlineInvalidMatchCode'));

  const fc = await ensureOnlineReady();
  setOnlineStatus('onlineStatusJoining', null, true);
  updateOnlineStatusUI();

  const user = fc.auth.currentUser;
  const uid = user?.uid || '';
  const guestName = user?.displayName || `Guest-${uid.slice(0, 6)}`;
  const matchRef = fc.doc(fc.db, 'matches', normalized);
  const txResult = await fc.runTransaction(fc.db, async (tx) => {
    const snap = await tx.get(matchRef);
    if (!snap.exists()) {
      throw new Error(t('onlineMatchNotFound'));
    }
    const data = snap.data() || {};
    let role = '';

    if (data.hostUid && data.hostUid === uid) {
      role = 'host';
    } else if (data.guestUid && data.guestUid === uid) {
      role = 'guest';
    } else if (!data.guestUid && (data.status === 'waiting' || !data.status)) {
      role = 'guest';
      tx.update(matchRef, {
        guestUid: uid || null,
        guestName,
        status: 'active',
        updatedAt: fc.serverTimestamp()
      });
      data.guestUid = uid;
      data.guestName = guestName;
      data.status = 'active';
    } else {
      throw new Error(t('onlineMatchFull'));
    }
    return { role, data };
  });

  clearOnlineSession(true);
  onlineSession.matchId = normalized;
  onlineSession.role = txResult.role;
  onlineSession.hostUid = txResult.data.hostUid || '';
  onlineSession.guestUid = txResult.data.guestUid || '';
  onlineSession.status = txResult.data.status || 'active';
  onlineSession.link = buildOnlineMatchLink(normalized);

  const cfg = onlineStartBaseConfig(txResult.data);
  selectedMode = cfg.mode;
  selectedDifficulty = cfg.difficulty;
  selectedSide = onlineRoleSide();

  if (sideSelect) sideSelect.value = selectedSide;
  if (difficultySelect) difficultySelect.value = selectedDifficulty;
  if (onlineMatchCodeEl) onlineMatchCodeEl.value = normalized;
  if (joinMatchInputEl) joinMatchInputEl.value = normalized;

  updateStatus();
  setOnlineStatus('onlineStatusJoined', { code: normalized }, true);
  await subscribeToOnlineMatch(normalized);
  await ensureOnlineRtcConnection(false);
  updateOnlineStatusUI();
}

async function startOnlineGame() {
  if (!onlineSession.matchId) throw new Error(t('onlineStatusNoSession'));
  const details = (onlineSession.role === 'host')
    ? await waitForOnlineGuestJoin(onlineSession.matchId, 12000)
    : await fetchOnlineMatchDoc(onlineSession.matchId);
  if (!details) throw new Error(t('onlineMatchNotFound'));
  const matchData = details.data || {};
  if (matchData.hostUid) onlineSession.hostUid = matchData.hostUid;
  if (matchData.guestUid) onlineSession.guestUid = matchData.guestUid;

  if (onlineSession.role === 'host' && !(matchData.guestUid || onlineSession.guestUid)) {
    throw new Error(t('onlineStatusNeedGuest'));
  }

  setOnlineStatus('onlineStatusConnecting', null, true);
  updateOnlineStatusUI();
  const linked = await ensureOnlineRtcConnection(true);
  if (!linked && onlineSession.role !== 'host') throw new Error(t('onlineStatusConnecting'));

  await restoreOnlineStateFromFirestore(matchData);
  selectedPlayerMode = 'online';
  selectedSide = onlineRoleSide();
  if (sideSelect) sideSelect.value = selectedSide;

  if (onlineSession.role === 'host') {
    await writeOnlineMatchStateUpdate({ move: null, ply: moveHistory.length - 1 });
    if (linked) {
      await sendOnlineSyncState('host_start');
      setOnlineStatus('onlineStatusConnected', null, true);
    } else {
      setOnlineStatus('onlineStatusConnecting', null, true);
    }
  } else {
    sendOnlineMessage({ t: 'sync_request', reason: 'guest_start' }, false);
    setOnlineStatus(linked ? 'onlineStatusConnected' : 'onlineStatusConnecting', null, true);
  }

  updateOnlineStatusUI();
}

async function tryAutoJoinOnlineMatchFromQuery() {
  const code = sanitizeMatchId(onlineQueryMatchCode);
  if (!code) return;
  selectedPlayerMode = 'online';
  if (joinMatchInputEl) joinMatchInputEl.value = code;
  updateSetupSelectionUI();
  try {
    await joinOnlineMatch(code);
  } catch (err) {
    console.warn('[Commander Chess] auto-join failed', err);
    setOnlineStatus('onlineMatchNotFound', null, true);
    updateOnlineStatusUI();
  }
}

let gameApi = null;
let gameApiInitPromise = null;
let localGameCounter = 0;

function nextLocalGameId() {
  localGameCounter += 1;
  return `local-${Date.now().toString(36)}-${localGameCounter.toString(36)}`;
}

function difficultyDepthForBot(difficulty) {
  if (difficulty === 'easy') return 4;
  if (difficulty === 'hard') return 8;
  return 6;
}

function difficultyTimeMsForBot(difficulty) {
  if (difficulty === 'easy') return 220;
  if (difficulty === 'hard') return 900;
  return 450;
}

async function cloudApiGet(path) {
  const res = await fetch(path);
  const data = await res.json();
  if (!res.ok) throw new Error(data?.error || `HTTP ${res.status}`);
  return data;
}

async function cloudApiPost(path, payload) {
  const res = await fetch(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload || {})
  });

  let data = null;
  try {
    data = await res.json();
  } catch (_) {
    data = null;
  }

  if (!res.ok) {
    const msg = data?.error || `HTTP ${res.status}`;
    throw new Error(msg);
  }

  return data;
}

const cloudGameApi = {
  name: 'cloud',
  async sprites() {
    return cloudApiGet('/api/sprites');
  },
  async newGame({ human_player, game_mode, difficulty }) {
    return cloudApiPost('/api/new', { human_player, game_mode, difficulty });
  },
  async move({ game_id, move }) {
    return cloudApiPost('/api/move', { game_id, move });
  },
  async bot({ game_id }) {
    return cloudApiPost('/api/bot', { game_id });
  }
};

async function createWasmGameApi() {
  const mod = await import('/engine/engine_bridge.js');
  const bridge = mod?.default || mod;
  if (!bridge || typeof bridge.initEngine !== 'function') {
    throw new Error('engine bridge unavailable');
  }

  await bridge.initEngine();

  return {
    name: 'wasm',
    async sprites() {
      const sprites = await bridge.getSprites();
      return { sprites: sprites || Object.create(null) };
    },
    async newGame({ human_player, game_mode, difficulty }) {
      const state = await bridge.newGame({
        gameMode: game_mode,
        difficulty,
        humanPlayer: human_player
      });
      return { game_id: nextLocalGameId(), state };
    },
    async move({ move }) {
      const state = await bridge.applyMove(move);
      if (!state || typeof state !== 'object') throw new Error('illegal move');
      return { state };
    },
    async bot({ difficulty }) {
      const move = await bridge.getBestMove({
        timeMs: difficultyTimeMsForBot(difficulty),
        depth: difficultyDepthForBot(difficulty)
      });
      if (!move || typeof move.pid !== 'number' || move.pid < 0) {
        throw new Error('bot could not find a legal move');
      }

      const state = await bridge.applyMove(move);
      if (!state || typeof state !== 'object') throw new Error('bot move was rejected');
      return { move, state };
    }
  };
}

async function ensureGameApi() {
  if (gameApi) return gameApi;
  if (gameApiInitPromise) return gameApiInitPromise;

  gameApiInitPromise = (async () => {
    try {
      gameApi = await createWasmGameApi();
      console.info('[Commander Chess] Engine mode: local WASM');
    } catch (err) {
      console.warn('[Commander Chess] Falling back to cloud API:', err);
      gameApi = cloudGameApi;
    }
    return gameApi;
  })();

  return gameApiInitPromise;
}

async function loadSprites() {
  try {
    const api = await ensureGameApi();
    const data = await api.sprites();
    spriteMap = data.sprites || Object.create(null);
  } catch (_) {
    spriteMap = Object.create(null);
  }
}

function clearAutoBotTimer() {
  if (autoBotTimer) {
    clearTimeout(autoBotTimer);
    autoBotTimer = null;
  }
}

function maybeAutoBotTurn() {
  clearAutoBotTimer();
  if (isLocalMultiplayer() || isOnlineMultiplayer()) return;
  if (!state || !gameId || state.game_over || botThinking) return;
  if (reviewIndex >= 0) return;
  if (state.turn === humanSide()) return;

  autoBotTimer = setTimeout(() => {
    requestBot(false).catch(err => showError(err, () => requestBot(false)));
  }, 260);
}

function updateBotButtonState() {
  if (!botBtn) return;
  const disabled = isLocalMultiplayer()
    || isOnlineMultiplayer()
    || !state
    || !gameId
    || state.game_over
    || botThinking
    || reviewIndex >= 0;
  botBtn.disabled = disabled;
}

function updateSetupRulesToggleLabel() {
  if (!setupRulesToggleBtn) return;
  const label = setupRulesMenuOpen ? t('setupRulesClose') : t('setupRulesOpen');
  setupRulesToggleBtn.textContent = setupRulesMenuOpen ? label : `☰ ${label}`;
  setupRulesToggleBtn.setAttribute('aria-expanded', setupRulesMenuOpen ? 'true' : 'false');
}

function setSetupRulesMenuOpen(open) {
  setupRulesMenuOpen = !!open;
  if (setupOverlayEl) setupOverlayEl.classList.toggle('mobile-rules-open', setupRulesMenuOpen);
  updateSetupRulesToggleLabel();
}

function updateSetupAboutToggleLabel() {
  if (!setupAboutToggleBtn) return;
  const label = t('setupAboutOpen');
  setupAboutToggleBtn.textContent = label;
}

function openSetupAboutModal(triggerEl = null) {
  if (!setupAboutModalEl) return;
  lastSetupAboutTrigger = triggerEl || (document.activeElement instanceof HTMLElement ? document.activeElement : null);
  setupAboutModalEl.classList.add('show');
  setupAboutModalEl.setAttribute('aria-hidden', 'false');
  window.setTimeout(() => {
    if (setupAboutCloseBtn) setupAboutCloseBtn.focus();
    else if (setupAboutCardEl) setupAboutCardEl.focus();
  }, 0);
}

function closeSetupAboutModal() {
  if (!setupAboutModalEl) return;
  setupAboutModalEl.classList.remove('show');
  setupAboutModalEl.setAttribute('aria-hidden', 'true');
  if (lastSetupAboutTrigger && typeof lastSetupAboutTrigger.focus === 'function') {
    lastSetupAboutTrigger.focus();
  }
  lastSetupAboutTrigger = null;
}

function setSetupDetailsVisible(open) {
  setupDetailsVisible = !!open;
  if (setupDetailOptionsEl) setupDetailOptionsEl.hidden = !setupDetailsVisible;
  if (startModeBtn) startModeBtn.hidden = !setupDetailsVisible;
  if (presetCustomBtn) {
    presetCustomBtn.classList.toggle('active', setupDetailsVisible);
    presetCustomBtn.setAttribute('aria-expanded', setupDetailsVisible ? 'true' : 'false');
  }
}

function clearSetupFlowCallbacks() {
  pendingRulesDocCloseAction = null;
  pendingTutorialCloseAction = null;
}

function initializeRulesAccordion() {
  if (!rulesAccordionEl) return;
  const accordionItems = Array.from(rulesAccordionEl.querySelectorAll('.rules-accordion-item'));
  for (const item of accordionItems) {
    item.addEventListener('toggle', () => {
      if (!item.open) return;
      for (const otherItem of accordionItems) {
        if (otherItem !== item) otherItem.open = false;
      }
    });
  }
}

function applySetupPreset({ theme = null, playerMode, mode, difficulty, side, autoStart = false }) {
  if (theme) applyTheme(theme, true);

  if (isValidPlayerMode(playerMode)) {
    const wasOnline = isOnlineMultiplayer();
    selectedPlayerMode = playerMode;
    if (wasOnline && !isOnlineMultiplayer() && state == null) {
      clearOnlineSession(true);
    }
  }
  if (isValidMode(mode)) selectedMode = mode;
  if (isValidDifficulty(difficulty)) selectedDifficulty = difficulty;
  if (side === 'red' || side === 'blue') selectedSide = side;

  if (sideSelect) sideSelect.value = selectedSide;
  if (difficultySelect) difficultySelect.value = selectedDifficulty;
  setSetupDetailsVisible(false);
  clearRetryAction();
  updateSetupSelectionUI();
  updateStatus();
  drawBoard();

  if (autoStart) {
    const launchConfig = currentLaunchConfig();
    runStartGame(launchConfig).catch((err) => showError(err, () => runStartGame(launchConfig)));
  }
}

function runQuickStartBeginnerFlow() {
  const launchConfig = {
    mode: 'full',
    side: 'blue',
    difficulty: 'easy',
    playerMode: 'single'
  };

  clearSetupFlowCallbacks();
  applySetupPreset({
    theme: 'system',
    playerMode: launchConfig.playerMode,
    mode: launchConfig.mode,
    difficulty: launchConfig.difficulty,
    side: launchConfig.side,
    autoStart: false
  });

  pendingTutorialCloseAction = () => {
    clearRetryAction();
    runStartGame(launchConfig).catch((err) => showError(err, () => runStartGame(launchConfig)));
  };
  pendingRulesDocCloseAction = () => {
    openTutorial({ replay: true });
  };
  openRulesDocModal(presetQuickStartBtn || null);
}

function applyLocalizedStaticText() {
  document.documentElement.lang = selectedLanguage;
  document.title = t('documentTitle');
  if (metaDescriptionTagEl) metaDescriptionTagEl.setAttribute('content', t('metaDescription'));
  if (ogTitleTagEl) ogTitleTagEl.setAttribute('content', t('ogTitle'));
  if (ogDescriptionTagEl) ogDescriptionTagEl.setAttribute('content', t('ogDescription'));
  if (ogUrlTagEl) ogUrlTagEl.setAttribute('content', window.location.href);

  if (setupKickerEl) setupKickerEl.textContent = t('setupKicker');
  if (setupTitleEl) setupTitleEl.textContent = t('setupTitle');
  if (setupSubEl) setupSubEl.textContent = t('setupSub');
  updateSetupAboutToggleLabel();
  if (setupAboutTitleEl) setupAboutTitleEl.textContent = t('about');
  if (setupAboutNameLabelEl) setupAboutNameLabelEl.textContent = t('aboutName');
  if (setupAboutPositionLabelEl) setupAboutPositionLabelEl.textContent = t('aboutPosition');
  if (setupAboutPositionValueEl) setupAboutPositionValueEl.textContent = t('aboutPositionValue');
  if (setupAboutGithubLabelEl) setupAboutGithubLabelEl.textContent = t('aboutGithub');
  if (setupAboutCloseBtn) setupAboutCloseBtn.textContent = t('rulesDocClose');
  if (presetQuickStartBtn) {
    const quickStrong = presetQuickStartBtn.querySelector('strong');
    const quickSpan = presetQuickStartBtn.querySelector('span');
    if (quickStrong) quickStrong.textContent = selectedLanguage === 'vi' ? 'Vào Nhanh (Người Mới)' : 'Quick Start (Beginner)';
    if (quickSpan) quickSpan.textContent = selectedLanguage === 'vi'
      ? '1 người · Full Battle · Beginner · Xem luật + hướng dẫn · Tự động bắt đầu'
      : 'Single · Full Battle · Beginner · Rulebook intro + tutorial · Auto start';
  }
  if (presetClassicBtn) {
    const classicStrong = presetClassicBtn.querySelector('strong');
    const classicSpan = presetClassicBtn.querySelector('span');
    if (classicStrong) classicStrong.textContent = selectedLanguage === 'vi' ? 'Tạo Trận' : 'Create Match';
    if (classicSpan) classicSpan.textContent = selectedLanguage === 'vi'
      ? 'Mở cửa sổ thiết lập trận (chế độ, phe, độ khó, online).'
      : 'Open full match setup window (mode, side, difficulty, online).';
  }
  if (presetCustomBtn) {
    const customStrong = presetCustomBtn.querySelector('strong');
    const customSpan = presetCustomBtn.querySelector('span');
    if (customStrong) customStrong.textContent = selectedLanguage === 'vi' ? '⚙️ Tùy Chỉnh Chi Tiết' : '⚙️ Custom Setup';
    if (customSpan) customSpan.textContent = selectedLanguage === 'vi'
      ? 'Hiện / ẩn các tùy chọn chi tiết'
      : 'Show / hide detailed options';
  }
  updateSetupRulesToggleLabel();
  if (languageLabelEl) languageLabelEl.textContent = t('languageLabel');
  if (setupThemeLabelEl) setupThemeLabelEl.textContent = t('themeField');
  if (playerModeLabelEl) playerModeLabelEl.textContent = t('playerModeLabel');
  if (onlinePanelTitleEl) onlinePanelTitleEl.textContent = t('onlinePanelTitle');
  if (onlinePanelHintEl) onlinePanelHintEl.textContent = t('onlinePanelHint');
  if (onlineMatchCodeLabelEl) onlineMatchCodeLabelEl.textContent = t('onlineMatchCodeLabel');
  if (joinMatchInputLabelEl) joinMatchInputLabelEl.textContent = t('onlineJoinLabel');
  if (joinMatchInputEl) joinMatchInputEl.placeholder = t('onlineJoinPlaceholder');
  if (createOnlineBtn) createOnlineBtn.textContent = t('onlineCreateBtn');
  if (joinOnlineBtn) joinOnlineBtn.textContent = t('onlineJoinBtn');
  if (copyOnlineCodeBtn) copyOnlineCodeBtn.textContent = t('onlineCopyCodeBtn');
  if (copyOnlineLinkBtn) copyOnlineLinkBtn.textContent = t('onlineCopyLinkBtn');
  if (rulesTitleEl) rulesTitleEl.textContent = t('rulesTitle');
  if (rulesSourceEl) rulesSourceEl.textContent = t('rulesSource');
  if (rulesSectionOverviewEl) rulesSectionOverviewEl.textContent = t('rulesSectionOverview');
  if (rulesSectionBattlefieldEl) rulesSectionBattlefieldEl.textContent = t('rulesSectionBattlefield');
  if (rulesSectionAllPiecesEl) rulesSectionAllPiecesEl.textContent = t('rulesSectionAllPieces');
  if (rulesSectionMovementCaptureEl) rulesSectionMovementCaptureEl.textContent = t('rulesSectionMovementCapture');
  if (rulesSectionSpecialRulesEl) rulesSectionSpecialRulesEl.textContent = t('rulesSectionSpecialRules');
  if (rulesSectionWinModesEl) rulesSectionWinModesEl.textContent = t('rulesSectionWinModes');
  if (rulesGoalEl) rulesGoalEl.textContent = t('rulesGoal');
  if (rulesBoardEl) rulesBoardEl.textContent = t('rulesBoard');
  if (rulesPiecesEl) rulesPiecesEl.textContent = t('rulesPieces');
  if (rulesMoveEl) rulesMoveEl.textContent = t('rulesMove');
  if (rulesCarryEl) rulesCarryEl.textContent = t('rulesCarry');
  if (rulesHeroEl) rulesHeroEl.textContent = t('rulesHero');
  if (rulesModesEl) rulesModesEl.textContent = t('rulesModes');
  if (rulesDetailsSummaryEl) rulesDetailsSummaryEl.textContent = t('rulesDetailsSummary');
  if (rulesDetail1El) rulesDetail1El.textContent = t('rulesDetail1');
  if (rulesDetail2El) rulesDetail2El.textContent = t('rulesDetail2');
  if (rulesDetail3El) rulesDetail3El.textContent = t('rulesDetail3');
  if (rulesDetail4El) rulesDetail4El.textContent = t('rulesDetail4');
  if (rulesDetail5El) rulesDetail5El.textContent = t('rulesDetail5');
  if (rulesDetail6El) rulesDetail6El.textContent = t('rulesDetail6');
  if (rulesDetail7El) rulesDetail7El.textContent = t('rulesDetail7');
  if (rulesDetail8El) rulesDetail8El.textContent = t('rulesDetail8');
  if (rulesDetail9El) rulesDetail9El.textContent = t('rulesDetail9');
  if (rulesDetail10El) rulesDetail10El.textContent = t('rulesDetail10');
  if (rulesDetail11El) rulesDetail11El.textContent = t('rulesDetail11');
  if (rulesDetail12El) rulesDetail12El.textContent = t('rulesDetail12');
  if (rulesDetailNoteEl) rulesDetailNoteEl.textContent = t('rulesDetailNote');
  if (rulesPiecesSummaryEl) rulesPiecesSummaryEl.textContent = t('rulesPiecesSummary');
  if (pieceRuleCEl) pieceRuleCEl.textContent = t('pieceRuleC');
  if (pieceRuleHEl) pieceRuleHEl.textContent = t('pieceRuleH');
  if (pieceRuleInEl) pieceRuleInEl.textContent = t('pieceRuleIn');
  if (pieceRuleMEl) pieceRuleMEl.textContent = t('pieceRuleM');
  if (pieceRuleTEl) pieceRuleTEl.textContent = t('pieceRuleT');
  if (pieceRuleEEl) pieceRuleEEl.textContent = t('pieceRuleE');
  if (pieceRuleAEl) pieceRuleAEl.textContent = t('pieceRuleA');
  if (pieceRuleAaEl) pieceRuleAaEl.textContent = t('pieceRuleAa');
  if (pieceRuleMsEl) pieceRuleMsEl.textContent = t('pieceRuleMs');
  if (pieceRuleAfEl) pieceRuleAfEl.textContent = t('pieceRuleAf');
  if (pieceRuleNEl) pieceRuleNEl.textContent = t('pieceRuleN');
  if (pieceRuleHeroEl) pieceRuleHeroEl.textContent = t('pieceRuleHero');
  if (pieceRuleCarryEl) pieceRuleCarryEl.textContent = t('pieceRuleCarry');
  if (openRulesDocBtn) openRulesDocBtn.textContent = t('rulesDocOpenBtn');
  if (rulesDocTitleEl) rulesDocTitleEl.textContent = t('rulesDocTitle');
  if (closeRulesDocBtn) closeRulesDocBtn.textContent = t('rulesDocClose');
  if (rulesDocHintEl) rulesDocHintEl.textContent = t('rulesDocHint');
  if (rulesDocDirectLinkEl) rulesDocDirectLinkEl.textContent = t('rulesDocLink');
  if (rulesDocDirectLinkEl) rulesDocDirectLinkEl.href = rulesDocUrl();
  if (rulesDocModalEl && rulesDocModalEl.classList.contains('show') && rulesDocFrameEl) {
    rulesDocFrameEl.src = rulesPageUrl();
  }
  if (mainKickerEl) mainKickerEl.textContent = t('mainKicker');
  if (newBtn) newBtn.textContent = t('newGameMenu');
  if (quickRestartBtn) quickRestartBtn.textContent = t('quickRestart');
  if (tutorialBtn) tutorialBtn.textContent = 'REPLAY TUTORIAL';
  if (tutorialKickerEl) tutorialKickerEl.textContent = 'Interactive Tutorial';
  if (skipTutorialBtn) skipTutorialBtn.textContent = 'SKIP TUTORIAL';
  if (tutorialPrevBtn) tutorialPrevBtn.textContent = 'BACK';
  if (tutorialReplayBtn) tutorialReplayBtn.textContent = 'REPLAY';
  if (tutorialStartBtn) tutorialStartBtn.textContent = 'START TUTORIAL';
  if (tutorialNextBtn) tutorialNextBtn.textContent = 'NEXT';
  if (quickTutorialOverlayEl) {
    const msgEl = quickTutorialOverlayEl.querySelector('p');
    if (msgEl) msgEl.textContent = selectedLanguage === 'vi'
      ? 'Ván đầu tiên? Học nhanh luật trong 60 giây.'
      : 'First battle? Learn the basics in 60 seconds.';
  }
  if (quickTutorialStartBtn) quickTutorialStartBtn.textContent = selectedLanguage === 'vi' ? 'BẮT ĐẦU HƯỚNG DẪN NHANH' : 'START QUICK TUTORIAL';
  if (quickTutorialSkipBtn) quickTutorialSkipBtn.textContent = selectedLanguage === 'vi' ? 'BỎ QUA' : 'SKIP';
  if (pwaInstallBtn) pwaInstallBtn.textContent = selectedLanguage === 'vi' ? 'THÊM VÀO MÀN HÌNH CHÍNH' : 'ADD TO HOME SCREEN';
  if (postGameRematchBtn) postGameRematchBtn.textContent = selectedLanguage === 'vi' ? 'CHƠI LẠI (GIỮ CÀI ĐẶT)' : 'REMATCH SAME SETTINGS';
  if (postGameShareBtn) postGameShareBtn.textContent = selectedLanguage === 'vi' ? 'SAO CHÉP LIÊN KẾT PHÁT LẠI' : 'COPY REPLAY LINK';
  if (postGameCloseBtn) postGameCloseBtn.textContent = selectedLanguage === 'vi' ? 'ĐÓNG' : 'CLOSE';
  updateStartModeButton();
  updateQuickRestartVisibility();
  if (botBtn) botBtn.textContent = t('forceBotMove');
  if (retryBtn) retryBtn.textContent = t('retryAction');
  setRetryAction(pendingRetryAction);
  updateFlipButtonLabel();
  if (riverLabelEl) riverLabelEl.textContent = t('riverLabel');
  if (boardEl) {
    boardEl.setAttribute('aria-label', t('boardAriaLabel'));
    boardEl.setAttribute('aria-roledescription', t('boardRoleDescription'));
  }

  if (controlDeckTitleEl) controlDeckTitleEl.textContent = t('controlDeck');
  if (sideFieldLabelEl) sideFieldLabelEl.textContent = t('playerSide');
  if (difficultyFieldLabelEl) difficultyFieldLabelEl.textContent = t('difficultyField');
  if (themeFieldLabelEl) themeFieldLabelEl.textContent = t('themeField');
  if (settingsTitleEl) settingsTitleEl.textContent = t('settingsTitle');
  if (boardSkinLabelEl) boardSkinLabelEl.textContent = t('boardSkinLabel');
  if (pieceThemeLabelEl) pieceThemeLabelEl.textContent = t('pieceThemeLabel');
  if (soundToggleLabelEl) soundToggleLabelEl.textContent = t('soundToggleLabel');
  if (soundVolumeLabelEl) soundVolumeLabelEl.textContent = t('soundVolumeLabel');
  if (animToggleLabelEl) animToggleLabelEl.textContent = t('animToggleLabel');
  if (moveHintsLabelEl) moveHintsLabelEl.textContent = t('moveHintsLabel');
  if (telemetryTitleEl) telemetryTitleEl.textContent = t('telemetry');
  if (historyTitleEl) historyTitleEl.textContent = t('moveHistory');
  if (histPrevBtn) histPrevBtn.setAttribute('aria-label', t('historyPrevAria'));
  if (histNextBtn) histNextBtn.setAttribute('aria-label', t('historyNextAria'));
  if (histLiveBtn) histLiveBtn.setAttribute('aria-label', t('historyLiveAria'));
  if (histLiveBtn) histLiveBtn.textContent = t('live');
  if (legendTitleEl) legendTitleEl.textContent = t('legend');
  if (legendLegalTextEl) legendLegalTextEl.textContent = t('legendLegal');
  if (legendSelectedTextEl) legendSelectedTextEl.textContent = t('legendSelected');
  if (legendCaptureTextEl) legendCaptureTextEl.textContent = t('legendCapture');
  if (legendLastMoveTextEl) legendLastMoveTextEl.textContent = t('legendLastMove');
  if (legendHeroTextEl) legendHeroTextEl.textContent = t('legendHero');
  if (aboutTitleEl) aboutTitleEl.textContent = t('about');
  if (aboutNameLabelEl) aboutNameLabelEl.textContent = t('aboutName');
  if (aboutPositionLabelEl) aboutPositionLabelEl.textContent = t('aboutPosition');
  if (aboutPositionValueEl) aboutPositionValueEl.textContent = t('aboutPositionValue');
  if (aboutGithubLabelEl) aboutGithubLabelEl.textContent = t('aboutGithub');
  if (aboutEmailLabelEl) aboutEmailLabelEl.textContent = t('aboutEmail');
  if (signInBtn) signInBtn.textContent = t('authSignInGoogle');
  if (signOutBtn) signOutBtn.textContent = t('authSignOut');
  updateAuthUI();

  if (statLabelYouEl) statLabelYouEl.textContent = t('statYou');
  if (statLabelDifficultyEl) statLabelDifficultyEl.textContent = t('statDifficulty');
  if (statLabelTurnEl) statLabelTurnEl.textContent = t('statTurn');
  if (statLabelLegalEl) statLabelLegalEl.textContent = t('statLegal');
  if (statLabelPiecesEl) statLabelPiecesEl.textContent = t('statPieces');
  if (statLabelSelectionEl) statLabelSelectionEl.textContent = t('statSelection');

  const languageLabels = textMap('languageLabelByCode');
  if (languageButtonsEl) {
    languageButtonsEl.querySelectorAll('.lang-btn').forEach((btn) => {
      const lang = btn.dataset.lang;
      btn.textContent = languageLabels[lang] || lang;
    });
  }

  const themeSetupLabels = textMap('themeLabelByCode');
  if (setupThemeButtonsEl) {
    setupThemeButtonsEl.querySelectorAll('.theme-btn').forEach((btn) => {
      const theme = btn.dataset.theme;
      btn.textContent = themeSetupLabels[theme] || theme;
    });
  }

  const playerModeLabels = textMap('playerModeLabelByCode');
  if (playerModeButtonsEl) {
    playerModeButtonsEl.querySelectorAll('.player-mode-btn').forEach((btn) => {
      const playerMode = btn.dataset.playerMode;
      btn.textContent = playerModeLabels[playerMode] || playerMode;
    });
  }

  if (modeButtonsEl) {
    modeButtonsEl.querySelectorAll('.mode-btn').forEach((btn) => {
      const mode = btn.dataset.mode;
      const strong = btn.querySelector('strong');
      const span = btn.querySelector('span');
      if (strong) strong.textContent = modeLabel(mode);
      if (span) span.textContent = modeDescription(mode);
    });
  }

  if (difficultyButtonsEl) {
    difficultyButtonsEl.querySelectorAll('.diff-btn').forEach((btn) => {
      const difficulty = btn.dataset.difficulty;
      const strong = btn.querySelector('strong');
      const span = btn.querySelector('span');
      if (strong) strong.textContent = difficultyLabel(difficulty);
      if (span) span.textContent = difficultyDescription(difficulty);
    });
  }

  const sideSetupLabels = textMap('sideSetupLabel');
  if (sideButtonsEl) {
    sideButtonsEl.querySelectorAll('.side-btn').forEach((btn) => {
      const side = btn.dataset.side;
      btn.textContent = sideSetupLabels[side] || side;
    });
  }

  const sideSelectLabels = textMap('sideSelectLabel');
  if (sideSelect) {
    sideSelect.querySelectorAll('option').forEach((opt) => {
      const side = opt.value;
      opt.textContent = sideSelectLabels[side] || side;
    });
  }

  if (difficultySelect) {
    difficultySelect.querySelectorAll('option').forEach((opt) => {
      opt.textContent = difficultyLabel(opt.value);
    });
  }

  const themeLabels = textMap('themeLabelByCode');
  if (themeSelect) {
    themeSelect.querySelectorAll('option').forEach((opt) => {
      opt.textContent = themeLabels[opt.value] || opt.value;
    });
    if (themeSelect.value !== selectedTheme) themeSelect.value = selectedTheme;
  }

  const boardSkinLabels = textMap('boardSkinLabelByCode');
  if (boardSkinSelect) {
    boardSkinSelect.querySelectorAll('option').forEach((opt) => {
      opt.textContent = boardSkinLabels[opt.value] || opt.value;
    });
  }

  const pieceThemeLabels = textMap('pieceThemeLabelByCode');
  if (pieceThemeSelect) {
    pieceThemeSelect.querySelectorAll('option').forEach((opt) => {
      opt.textContent = pieceThemeLabels[opt.value] || opt.value;
    });
  }

  applyUiPrefs(uiPrefs, false);
  if (tutorialActive) renderTutorialStep();

  updateOnlineStatusUI();
}

function updateSetupSelectionUI() {
  if (isOnlineMultiplayer()) {
    selectedSide = onlineRoleSide();
    if (sideSelect) sideSelect.value = selectedSide;
  }
  if (languageButtonsEl) {
    languageButtonsEl.querySelectorAll('.lang-btn').forEach(btn => {
      btn.classList.toggle('active', btn.dataset.lang === selectedLanguage);
    });
  }
  if (setupThemeButtonsEl) {
    setupThemeButtonsEl.querySelectorAll('.theme-btn').forEach(btn => {
      btn.classList.toggle('active', btn.dataset.theme === selectedTheme);
    });
  }
  if (playerModeButtonsEl) {
    playerModeButtonsEl.querySelectorAll('.player-mode-btn').forEach(btn => {
      btn.classList.toggle('active', btn.dataset.playerMode === selectedPlayerMode);
    });
  }
  modeButtonsEl.querySelectorAll('.mode-btn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.mode === selectedMode);
  });
  const sharedMode = isLocalMultiplayer() || isOnlineMultiplayer();
  sideButtonsEl.querySelectorAll('.side-btn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.side === selectedSide);
    btn.disabled = sharedMode;
  });
  if (difficultyButtonsEl) {
    difficultyButtonsEl.classList.toggle('is-disabled', sharedMode);
    difficultyButtonsEl.querySelectorAll('.diff-btn').forEach(btn => {
      btn.classList.toggle('active', btn.dataset.difficulty === selectedDifficulty);
      btn.disabled = sharedMode;
    });
  }
  if (sideSelect) sideSelect.disabled = sharedMode;
  if (difficultySelect) difficultySelect.disabled = sharedMode;
  updateOnlinePanelVisibility();
  updateOnlineStatusUI();
  updateBotButtonState();
}

function openSetupMenu() {
  clearSetupFlowCallbacks();
  setSetupDetailsVisible(false);
  setSetupRulesMenuOpen(false);
  setupOverlayEl.classList.add('show');
  if (joinMatchInputEl && onlineQueryMatchCode && !joinMatchInputEl.value) {
    joinMatchInputEl.value = onlineQueryMatchCode;
  }
  updateSetupSelectionUI();
}

function closeSetupMenu() {
  setSetupRulesMenuOpen(false);
  closeSetupAboutModal();
  setupOverlayEl.classList.remove('show');
  closeRulesDocModal();
}

async function newGame(gameMode, side, difficulty, playerMode = selectedPlayerMode) {
  clearAutoBotTimer();
  clearBoardFx();
  closePostGameModal();
  postGameShownSignature = '';
  botThinking = false;
  guestMovePending = false;
  selectedPid = null;
  lastMoveFrom = null;
  selectedPlayerMode = isValidPlayerMode(playerMode) ? playerMode : 'single';
  const launchSide = (isLocalMultiplayer() || isOnlineMultiplayer()) ? 'red' : side;
  const api = await ensureGameApi();

  const data = await api.newGame({
    human_player: launchSide,
    game_mode: gameMode,
    difficulty
  });

  gameId = data.game_id;
  state = data.state;
  hasStartedGame = true;
  if (!isLocalMultiplayer() && !isOnlineMultiplayer() && launchSide === 'blue' && state && state.has_last_move) {
    playSound('cpu');
    playSound(state.last_move_capture ? 'capture' : 'move');
    if (state.game_over) playSound('win');
  }
  stateHistory = [cloneState(state)];
  moveHistory = [];
  reviewIndex = -1;
  updateHistoryUI();
  if (sideSelect) sideSelect.value = isOnlineMultiplayer() ? onlineRoleSide() : launchSide;
  selectedDifficulty = (state?.difficulty && isValidDifficulty(state.difficulty)) ? state.difficulty : difficulty;
  if (difficultySelect) difficultySelect.value = selectedDifficulty;
  selectedSide = isOnlineMultiplayer() ? onlineRoleSide() : launchSide;
  selectedMode = gameMode;

  updateSetupSelectionUI();
  updateQuickRestartVisibility();
  clearRetryAction();
  updateStatus();
  drawBoard();
  maybeAutoBotTurn();
}

async function sendMove(pid, dc, dr, options = null) {
  const opts = options && typeof options === 'object' ? options : Object.create(null);
  const skipAutoBot = !!opts.skipAutoBot;
  const prevState = cloneState(state);
  const api = await ensureGameApi();
  const data = await api.move({
    game_id: gameId,
    move: { pid, dc, dr }
  });

  playTransitionSounds(prevState, data.state);
  lastMoveFrom = findLastMoveFrom(prevState, data.state);
  rememberBoardFx(data.state, lastMoveFrom);
  addHistoryEntry(prevState, data.state, lastMoveFrom);
  updateHistoryUI();
  state = data.state;
  selectedPid = null;
  clearRetryAction();
  updateStatus();
  drawBoard();
  runMoveDelight(prevState, data.state, lastMoveFrom);
  if (!skipAutoBot) maybeAutoBotTurn();
  return data.state;
}

async function requestBot(force) {
  if (isLocalMultiplayer() || isOnlineMultiplayer()) return;
  if (!gameId || !state || state.game_over) return;
  if (!force && state.turn === humanSide()) return;
  if (botThinking) return;

  clearAutoBotTimer();
  playSound('cpu');
  botThinking = true;
  updateStatus();
  const prevState = cloneState(state);

  try {
    const api = await ensureGameApi();
    const data = await api.bot({ game_id: gameId, difficulty: activeDifficulty() });
    playTransitionSounds(prevState, data.state);
    lastMoveFrom = findLastMoveFrom(prevState, data.state);
    rememberBoardFx(data.state, lastMoveFrom);
    addHistoryEntry(prevState, data.state, lastMoveFrom);
    updateHistoryUI();
    state = data.state;
    selectedPid = null;
    clearRetryAction();
  } finally {
    botThinking = false;
    updateStatus();
    drawBoard();
    runMoveDelight(prevState, state, lastMoveFrom);
  }

  maybeAutoBotTurn();
}

function onCellClick(ev) {
  if (tutorialActive) return;
  if (!state || !gameId || state.game_over || botThinking) return;
  if (isOnlineMultiplayer() && guestMovePending) return;
  if (reviewIndex >= 0) {
    reviewIndex = -1;
    selectedPid = null;
    updateHistoryUI();
    updateStatus();
    drawBoard();
    return;
  }
  if (!isLocalMultiplayer() && state.turn !== humanSide()) return;

  const c = Number(ev.currentTarget.dataset.col);
  const r = Number(ev.currentTarget.dataset.row);
  const p = pieceAt(c, r);

  if (selectedPid != null) {
    const legal = legalMovesForPid(selectedPid);
    const chosen = legal.find(m => m.dc === c && m.dr === r);
    if (chosen) {
      const retryPid = selectedPid;
      const retryDc = chosen.dc;
      const retryDr = chosen.dr;
      if (isOnlineMultiplayer()) {
        const move = { pid: retryPid, dc: retryDc, dr: retryDr };
        if (onlineSession.role === 'host') {
          applyOnlineAuthoritativeMove(move, 'host')
            .catch((err) => showError(err, () => applyOnlineAuthoritativeMove(move, 'host')));
        } else {
          if (!onlineRtc || !onlineRtc.isReady()) {
            showError(new Error(t('onlineStatusConnecting')), () => startOnlineGame());
            return;
          }
          guestMovePending = true;
          selectedPid = null;
          updateStatus();
          drawBoard();
          const sent = sendOnlineMessage({
            t: 'move',
            authoritative: false,
            by: 'guest',
            ply: moveHistory.length,
            move,
            stateHash: stateHash(state)
          }, false);
          if (!sent) {
            guestMovePending = false;
            updateStatus();
            showError(new Error(t('onlineStatusConnecting')), () => startOnlineGame());
          }
        }
      } else {
        sendMove(retryPid, retryDc, retryDr).catch(err => showError(err, () => sendMove(retryPid, retryDc, retryDr)));
      }
      return;
    }
  }

  if (p && p.player === state.turn) selectedPid = p.id;
  else selectedPid = null;

  if (selectedPid != null && legalMovesForPid(selectedPid).length === 0) {
    playSound('invalid');
  }

  drawBoard();
  updateStatus();
}

newBtn.addEventListener('click', () => {
  if (state !== null) {
    const proceed = window.confirm(t('newGameConfirmPrompt'));
    if (!proceed) return;
  }
  clearRetryAction();
  openSetupMenu();
});

function currentLaunchConfig() {
  return {
    mode: selectedMode,
    side: selectedSide,
    difficulty: selectedDifficulty,
    playerMode: selectedPlayerMode
  };
}

async function launchGameFromSelection(config = null) {
  const selectedConfig = config || currentLaunchConfig();
  await newGame(
    selectedConfig.mode,
    selectedConfig.side,
    selectedConfig.difficulty,
    selectedConfig.playerMode
  );
  closeSetupMenu();
}

async function runStartGame(config = null) {
  if (startGamePending) return;
  startGamePending = true;
  updateStartModeButton();
  try {
    if (isOnlineMultiplayer()) {
      await startOnlineGame();
      closeSetupMenu();
    } else {
      if (onlineSession.matchId || onlineRtc || onlineMatchUnsub) {
        clearOnlineSession(true);
      }
      await launchGameFromSelection(config);
    }
  } finally {
    startGamePending = false;
    updateStartModeButton();
    updateOnlineStatusUI();
  }
}

startModeBtn.addEventListener('click', () => {
  const launchConfig = currentLaunchConfig();
  clearRetryAction();
  runStartGame(launchConfig).catch(err => showError(err, () => runStartGame(launchConfig)));
});

if (setupRulesToggleBtn) {
  setupRulesToggleBtn.addEventListener('click', () => {
    setSetupRulesMenuOpen(!setupRulesMenuOpen);
  });
}

if (setupAboutToggleBtn) {
  setupAboutToggleBtn.addEventListener('click', (ev) => {
    openSetupAboutModal(ev.currentTarget);
  });
}

if (setupAboutCloseBtn) {
  setupAboutCloseBtn.addEventListener('click', () => {
    closeSetupAboutModal();
  });
}

if (setupAboutModalEl) {
  setupAboutModalEl.addEventListener('click', (ev) => {
    if (ev.target === setupAboutModalEl) closeSetupAboutModal();
  });
}

if (quickRestartBtn) {
  quickRestartBtn.addEventListener('click', () => {
    if (!hasStartedGame) return;
    const launchConfig = currentLaunchConfig();
    clearRetryAction();
    runStartGame(launchConfig).catch(err => showError(err, () => runStartGame(launchConfig)));
  });
}

if (presetQuickStartBtn) {
  presetQuickStartBtn.addEventListener('click', () => {
    runQuickStartBeginnerFlow();
  });
}

if (presetClassicBtn) {
  presetClassicBtn.addEventListener('click', () => {
    clearSetupFlowCallbacks();
    setSetupDetailsVisible(true);
    if (setupDetailOptionsEl) {
      setupDetailOptionsEl.scrollIntoView({ block: 'start', behavior: 'smooth' });
    }
  });
}

if (languageButtonsEl) {
  languageButtonsEl.addEventListener('click', (ev) => {
    const btn = ev.target.closest('.lang-btn');
    if (!btn) return;
    const language = btn.dataset.lang;
    if (!isValidLanguage(language)) return;
    selectedLanguage = language;
    applyLocalizedStaticText();
    updateSetupSelectionUI();
    updateHistoryUI();
    updateStatus();
    drawBoard();
  });
}

if (setupThemeButtonsEl) {
  setupThemeButtonsEl.addEventListener('click', (ev) => {
    const btn = ev.target.closest('.theme-btn');
    if (!btn) return;
    const theme = btn.dataset.theme;
    if (!isValidTheme(theme)) return;
    applyTheme(theme, true);
    updateSetupSelectionUI();
  });
}

if (playerModeButtonsEl) {
  playerModeButtonsEl.addEventListener('click', (ev) => {
    const btn = ev.target.closest('.player-mode-btn');
    if (!btn) return;
    const playerMode = btn.dataset.playerMode;
    if (!isValidPlayerMode(playerMode)) return;

    const wasOnline = isOnlineMultiplayer();
    selectedPlayerMode = playerMode;
    if (isLocalMultiplayer()) {
      clearAutoBotTimer();
      selectedSide = 'red';
      if (sideSelect) sideSelect.value = selectedSide;
    } else if (isOnlineMultiplayer()) {
      clearAutoBotTimer();
      selectedSide = onlineRoleSide();
      if (sideSelect) sideSelect.value = selectedSide;
      if (!onlineStatusText) setOnlineStatus('onlineStatusIdle', null, true);
      if (joinMatchInputEl && onlineQueryMatchCode && !joinMatchInputEl.value) {
        joinMatchInputEl.value = onlineQueryMatchCode;
      }
    } else if (wasOnline && state == null) {
      clearOnlineSession(true);
    }

    updateSetupSelectionUI();
    updateStatus();
    maybeAutoBotTurn();
  });
}

if (createOnlineBtn) {
  createOnlineBtn.addEventListener('click', () => {
    selectedPlayerMode = 'online';
    updateSetupSelectionUI();
    createOnlineMatch().catch((err) => {
      showError(err, () => createOnlineMatch());
    });
  });
}

if (joinOnlineBtn) {
  joinOnlineBtn.addEventListener('click', () => {
    selectedPlayerMode = 'online';
    updateSetupSelectionUI();
    const matchCode = sanitizeMatchId(joinMatchInputEl ? joinMatchInputEl.value : '');
    if (!matchCode) {
      showError(new Error(t('onlineInvalidMatchCode')));
      return;
    }
    onlineQueryMatchCode = matchCode;
    joinOnlineMatch(matchCode).catch((err) => {
      showError(err, () => joinOnlineMatch(matchCode));
    });
  });
}

if (joinMatchInputEl) {
  joinMatchInputEl.addEventListener('keydown', (ev) => {
    if (ev.key !== 'Enter') return;
    ev.preventDefault();
    if (!joinOnlineBtn) return;
    joinOnlineBtn.click();
  });
}

if (copyOnlineCodeBtn) {
  copyOnlineCodeBtn.addEventListener('click', () => {
    if (!onlineSession.matchId) return;
    copyTextToClipboard(onlineSession.matchId, 'onlineCodeCopied').catch(() => {
      setOnlineStatus('onlineClipboardFailed', null, true);
      updateOnlineStatusUI();
    });
  });
}

if (copyOnlineLinkBtn) {
  copyOnlineLinkBtn.addEventListener('click', () => {
    if (!onlineSession.link) return;
    copyTextToClipboard(onlineSession.link, 'onlineLinkCopied').catch(() => {
      setOnlineStatus('onlineClipboardFailed', null, true);
      updateOnlineStatusUI();
    });
  });
}

modeButtonsEl.addEventListener('click', (ev) => {
  const btn = ev.target.closest('.mode-btn');
  if (!btn) return;
  selectedMode = btn.dataset.mode;
  updateSetupSelectionUI();
  updateStatus();
});

sideButtonsEl.addEventListener('click', (ev) => {
  const btn = ev.target.closest('.side-btn');
  if (!btn) return;
  if (isLocalMultiplayer() || isOnlineMultiplayer()) return;
  selectedSide = btn.dataset.side;
  sideSelect.value = selectedSide;
  updateSetupSelectionUI();
  updateStatus();
});

if (difficultyButtonsEl) {
  difficultyButtonsEl.addEventListener('click', (ev) => {
    const btn = ev.target.closest('.diff-btn');
    if (!btn) return;
    if (isLocalMultiplayer() || isOnlineMultiplayer()) return;
    selectedDifficulty = btn.dataset.difficulty;
    if (difficultySelect) difficultySelect.value = selectedDifficulty;
    updateSetupSelectionUI();
    updateStatus();
  });
}

botBtn.addEventListener('click', () => {
  requestBot(true).catch(err => showError(err, () => requestBot(true)));
});

if (retryBtn) {
  retryBtn.addEventListener('click', () => {
    if (!pendingRetryAction) return;
    const retryAction = pendingRetryAction;
    retryBtn.disabled = true;
    Promise.resolve()
      .then(() => retryAction())
      .catch(err => showError(err, retryAction));
  });
}

if (signInBtn) {
  signInBtn.addEventListener('click', () => {
    handleGoogleSignIn().catch((err) => {
      showError(err, () => handleGoogleSignIn());
    });
  });
}

if (signOutBtn) {
  signOutBtn.addEventListener('click', () => {
    handleSignOut().catch((err) => {
      showError(err, () => handleSignOut());
    });
  });
}

if (tutorialBtn) {
  tutorialBtn.addEventListener('click', () => {
    openTutorial({ replay: true, auto: false });
  });
}

if (quickTutorialStartBtn) {
  quickTutorialStartBtn.addEventListener('click', () => {
    setQuickTutorialPromptSeen(true);
    updateQuickTutorialOverlay();
    openTutorial({ replay: true, auto: false });
  });
}

if (quickTutorialSkipBtn) {
  quickTutorialSkipBtn.addEventListener('click', () => {
    setQuickTutorialPromptSeen(true);
    updateQuickTutorialOverlay();
  });
}

if (skipTutorialBtn) {
  skipTutorialBtn.addEventListener('click', () => {
    closeTutorial(true);
  });
}

if (tutorialPrevBtn) {
  tutorialPrevBtn.addEventListener('click', () => {
    prevTutorial();
  });
}

if (tutorialStartBtn) {
  tutorialStartBtn.addEventListener('click', () => {
    tutorialStep = Math.min(1, TUTORIAL_STEPS.length - 1);
    renderTutorialStep();
  });
}

if (tutorialNextBtn) {
  tutorialNextBtn.addEventListener('click', () => {
    nextTutorial();
  });
}

if (tutorialReplayBtn) {
  tutorialReplayBtn.addEventListener('click', () => {
    resetTutorialState();
    renderTutorialStep();
  });
}

if (tutorialModalEl) {
  tutorialModalEl.addEventListener('click', (ev) => {
    if (ev.target === tutorialModalEl) closeTutorial(true);
  });
}

if (postGameRematchBtn) {
  postGameRematchBtn.addEventListener('click', () => {
    const launchConfig = currentLaunchConfig();
    closePostGameModal();
    clearRetryAction();
    runStartGame(launchConfig).catch((err) => showError(err, () => runStartGame(launchConfig)));
  });
}

if (postGameShareBtn) {
  postGameShareBtn.addEventListener('click', async () => {
    const replayLink = buildReplayLink();
    try {
      if (navigator.clipboard && typeof navigator.clipboard.writeText === 'function') {
        await navigator.clipboard.writeText(replayLink);
      } else {
        const area = document.createElement('textarea');
        area.value = replayLink;
        area.style.position = 'fixed';
        area.style.left = '-1000px';
        area.style.top = '-1000px';
        document.body.appendChild(area);
        area.focus();
        area.select();
        const ok = document.execCommand('copy');
        document.body.removeChild(area);
        if (!ok) throw new Error('copy-failed');
      }
      setPostGameShareStatus(selectedLanguage === 'vi' ? 'Đã sao chép liên kết phát lại.' : 'Replay link copied.', true);
    } catch (_) {
      setPostGameShareStatus(selectedLanguage === 'vi'
        ? 'Không thể sao chép tự động. Hãy sao chép thủ công từ thanh địa chỉ.'
        : 'Auto-copy failed. Please copy the URL manually.');
    }
  });
}

if (postGameCloseBtn) {
  postGameCloseBtn.addEventListener('click', () => {
    closePostGameModal();
  });
}

if (postGameModalEl) {
  postGameModalEl.addEventListener('click', (ev) => {
    if (ev.target === postGameModalEl) closePostGameModal();
  });
}

if (flipBtn) {
  flipBtn.addEventListener('click', () => {
    if (resetBoardViewState()) return;
    boardFlipped = true;
    renderCoords();
    drawBoard();
    updateFlipButtonLabel();
  });
}

if (openRulesDocBtn) {
  openRulesDocBtn.addEventListener('click', (ev) => {
    openRulesDocModal(ev.currentTarget);
  });
}

if (closeRulesDocBtn) {
  closeRulesDocBtn.addEventListener('click', () => {
    closeRulesDocModal();
  });
}

if (rulesDocModalEl) {
  rulesDocModalEl.addEventListener('click', (ev) => {
    if (ev.target === rulesDocModalEl) closeRulesDocModal();
  });
}

if (boardFrameEl) {
  boardFrameEl.addEventListener('touchstart', onBoardPinchStart, { passive: false });
  boardFrameEl.addEventListener('touchmove', onBoardPinchMove, { passive: false });
  boardFrameEl.addEventListener('touchend', onBoardPinchEnd);
  boardFrameEl.addEventListener('touchcancel', onBoardPinchEnd);
}

sideSelect.addEventListener('change', () => {
  if (isLocalMultiplayer() || isOnlineMultiplayer()) return;
  selectedSide = sideSelect.value;
  selectedPid = null;
  updateSetupSelectionUI();
  updateStatus();
  drawBoard();
  maybeAutoBotTurn();
});

if (difficultySelect) {
  difficultySelect.addEventListener('change', () => {
    if (isLocalMultiplayer() || isOnlineMultiplayer()) return;
    selectedDifficulty = difficultySelect.value;
    updateSetupSelectionUI();
    updateStatus();
  });
}

if (themeSelect) {
  themeSelect.addEventListener('change', () => {
    const theme = themeSelect.value;
    if (!isValidTheme(theme)) return;
    applyTheme(theme, true);
    updateSetupSelectionUI();
  });
}

if (boardSkinSelect) {
  boardSkinSelect.addEventListener('change', () => {
    const boardSkin = boardSkinSelect.value;
    if (!isValidBoardSkin(boardSkin)) return;
    applyUiPrefs({ ...uiPrefs, boardSkin }, true);
    drawBoard();
  });
}

if (pieceThemeSelect) {
  pieceThemeSelect.addEventListener('change', () => {
    const pieceTheme = pieceThemeSelect.value;
    if (!isValidPieceTheme(pieceTheme)) return;
    applyUiPrefs({ ...uiPrefs, pieceTheme }, true);
    drawBoard();
  });
}

if (soundToggleEl) {
  soundToggleEl.addEventListener('change', () => {
    unlockAudio();
    applyUiPrefs({ ...uiPrefs, soundEnabled: !!soundToggleEl.checked }, true);
  });
}

if (soundVolumeRangeEl) {
  const commitVolume = () => {
    const volume = Math.max(0, Math.min(1, Number(soundVolumeRangeEl.value) / 100));
    applyUiPrefs({ ...uiPrefs, soundVolume: volume }, true);
  };
  soundVolumeRangeEl.addEventListener('input', commitVolume);
  soundVolumeRangeEl.addEventListener('change', commitVolume);
}

if (animToggleEl) {
  animToggleEl.addEventListener('change', () => {
    applyUiPrefs({ ...uiPrefs, animationsEnabled: !!animToggleEl.checked }, true);
    drawBoard();
  });
}

if (moveHintsToggleEl) {
  moveHintsToggleEl.addEventListener('change', () => {
    applyUiPrefs({ ...uiPrefs, moveHints: !!moveHintsToggleEl.checked }, true);
    drawBoard();
  });
}

histPrevBtn.addEventListener('click', () => {
  if (stateHistory.length <= 1) return;
  if (reviewIndex < 0) reviewIndex = stateHistory.length - 2;
  else if (reviewIndex > 0) reviewIndex--;
  selectedPid = null;
  updateHistoryUI();
  updateStatus();
  drawBoard();
});

histNextBtn.addEventListener('click', () => {
  if (reviewIndex < 0) return;
  if (reviewIndex < stateHistory.length - 1) reviewIndex++;
  if (reviewIndex >= stateHistory.length - 1) reviewIndex = -1;
  selectedPid = null;
  updateHistoryUI();
  updateStatus();
  drawBoard();
});

histLiveBtn.addEventListener('click', () => {
  if (reviewIndex < 0) return;
  reviewIndex = -1;
  selectedPid = null;
  updateHistoryUI();
  updateStatus();
  drawBoard();
});

initializeThemePreference();
initializeUiPrefs();
setBoardScale(1, { skipLabelUpdate: true });
setSetupRulesMenuOpen(false);
initializeRulesAccordion();
renderCoords();
setOnlineStatus('onlineStatusIdle', null, true);
applyLocalizedStaticText();
updateSetupSelectionUI();
updateQuickRestartVisibility();
openSetupMenu();
updatePwaInstallButton();
updateHistoryUI();
updateStatus();
initializeAuth().catch((err) => {
  console.warn('[Commander Chess] auth setup failed', err);
});

if (onlineQueryMatchCode) {
  tryAutoJoinOnlineMatchFromQuery().catch((err) => {
    console.warn('[Commander Chess] query match join failed', err);
  });
}

window.addEventListener('pointerdown', unlockAudio, { once: true });
window.addEventListener('keydown', unlockAudio, { once: true });
window.addEventListener('keydown', (ev) => {
  if (ev.key !== 'Escape') return;
  if (setupAboutModalEl && setupAboutModalEl.classList.contains('show')) {
    closeSetupAboutModal();
    return;
  }
  if (tutorialActive) {
    closeTutorial(true);
    return;
  }
  if (postGameModalEl && postGameModalEl.classList.contains('show')) {
    closePostGameModal();
  }
});
window.addEventListener('resize', () => {
  if (window.innerWidth > 768 && setupRulesMenuOpen) {
    setSetupRulesMenuOpen(false);
  }
});
window.addEventListener('beforeunload', () => {
  clearOnlineMatchSubscription();
  closeOnlineRtc();
  clearTutorialDragState();
  if (authUnsub) authUnsub();
});

window.addEventListener('beforeinstallprompt', (ev) => {
  ev.preventDefault();
  deferredInstallPrompt = ev;
  updatePwaInstallButton();
});

window.addEventListener('appinstalled', () => {
  deferredInstallPrompt = null;
  updatePwaInstallButton();
});

if (pwaInstallBtn) {
  pwaInstallBtn.addEventListener('click', async () => {
    if (!deferredInstallPrompt) return;
    try {
      deferredInstallPrompt.prompt();
      await deferredInstallPrompt.userChoice;
    } catch (_) {
      // user dismissed
    } finally {
      deferredInstallPrompt = null;
      updatePwaInstallButton();
    }
  });
}

if ('serviceWorker' in navigator) {
  window.addEventListener('load', () => {
    navigator.serviceWorker.register('/sw.js').catch((err) => {
      console.warn('[Commander Chess] service worker registration failed', err);
    });
  });
}

tryLoadReplayFromUrl().catch((err) => {
  console.warn('[Commander Chess] replay boot load failed', err);
});

loadSprites().then(() => drawBoard());
