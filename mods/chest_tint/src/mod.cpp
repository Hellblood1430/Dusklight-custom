// Chest Tint : teinte les coffres selon leur contenu, UNIQUEMENT en randomizer
// (dore = objet important, vert = insecte, violet = ame de Poe, blanc = objet Archipelago d'un autre joueur,
// bleu = petite cle, rouge = coeur).
//
// Principe :
//  1. Un hook sur daTbox_c::Draw note quel coffre est en train d'etre dessine.
//  2. Un hook sur setLightTevColorType_MAJI (la fonction qui applique les couleurs d'eclairage
//     au modele) modifie legerement les couleurs du coffre selon son contenu (voir
//     tint_for_item plus bas).
//
// Le contenu lu est celui du coffre APRES le passage du randomizer (le jeu ecrit l'objet
// randomise dans le coffre a sa creation), donc getItemNo() donne deja le bon objet.

#include "global.h"

#include "d/actor/d_a_tbox.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_kankyo.h"
#include "mods/items.h"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/item.h"
#include "mods/svc/log.h"

#include <cstdio>

DEFINE_MOD();
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(ItemService, svc_item);
IMPORT_SERVICE(LogService, svc_log);

namespace {

// ---------------------------------------------------------------------------------------------
// REGLAGES : modifie ces valeurs pour changer le rendu.
// Les valeurs sont AJOUTEES aux couleurs du coffre (plus c'est haut, plus c'est marque).
// Or / jaune : beaucoup de rouge, un peu moins de vert, pas de bleu.
// ---------------------------------------------------------------------------------------------
struct Tint {
    // Ajoute a la couleur ambiante du coffre (0 a 255).
    int ambR, ambG, ambB;
    // Ajoute a la couleur TEV du coffre (-1024 a 1023).
    int tevR, tevG, tevB;
};

constexpr Tint TINT_GOLD = {140, 105, 0, 90, 65, 0};  // objets importants : dore
constexpr Tint TINT_GREEN = {0, 90, 10, 0, 60, 5};    // insectes : vert
constexpr Tint TINT_PURPLE = {70, 0, 95, 45, 0, 65};  // ames de Poe : violet
constexpr Tint TINT_BLUE = {0, 45, 120, 0, 30, 80};    // petites cles : bleu
constexpr Tint TINT_RED = {120, 0, 15, 80, 0, 10};     // coeurs : rouge
constexpr Tint TINT_WHITE = {120, 120, 120, 80, 80, 80};  // objet d'un autre joueur (Archipelago) : blanc

// ---------------------------------------------------------------------------------------------
// LISTE DES OBJETS IMPORTANTS : ce sont les objets que le randomizer classe "Major", avec SES
// numeros d'objets (differents de ceux du jeu de base pour les cles, l'epee progressive, etc.).
// Ajoute ou retire des lignes a ta guise. Les insectes (vert), ames de Poe (violet),
// petites cles (bleu) et coeurs (rouge) ont leur propre couleur : ils ne sont pas dans cette liste.
// ---------------------------------------------------------------------------------------------
bool is_important_item(int item) {
    switch (item) {
    // Equipement
    case 0x2A:  // Ordon Shield
    case 0x2C:  // Hylian Shield
    case 0x30:  // Magic Armor
    case 0x31:  // Zora Armor
    case 0x32:  // Shadow Crystal
    case 0x35:  // Progressive Wallet
    case 0x3F:  // Progressive Sword
    // Objets de progression
    case 0x40:  // Gale Boomerang
    case 0x41:  // Spinner
    case 0x42:  // Ball and Chain
    case 0x43:  // Progressive Bow
    case 0x44:  // Progressive Clawshot
    case 0x45:  // Iron Boots
    case 0x46:  // Progressive Dominion Rod
    case 0x48:  // Lantern
    case 0x4A:  // Progressive Fishing Rod
    case 0x4B:  // Slingshot
    case 0x50:  // Bomb Bag
    case 0x60:  // Empty Bottle
    case 0x65:  // Bottle with Half Milk
    case 0x75:  // Bottle with Great Fairy's Tears
    case 0x9D:  // Bottle with Lantern Oil
    // Quetes et objets d'histoire
    case 0x80:  // Renado's Letter
    case 0x81:  // Invoice
    case 0x82:  // Wooden Statue
    case 0x83:  // Ilia's Charm
    case 0x90:  // Auru's Memo
    case 0x91:  // Ashei's Sketch
    case 0xA5:  // Progressive Mirror Shard
    case 0xD8:  // Progressive Fused Shadow
    case 0xE1:  // Progressive Hidden Skill
    case 0xE9:  // Progressive Sky Book
    // Cles speciales
    case 0x8E:  // Gerudo Desert Bulblin Camp Key
    case 0x92:  // Forest Temple Big Key
    case 0x93:  // Lakebed Temple Big Key
    case 0x94:  // Arbiter's Grounds Big Key
    case 0x95:  // Temple of Time Big Key
    case 0x96:  // City in the Sky Big Key
    case 0x97:  // Palace of Twilight Big Key
    case 0x98:  // Hyrule Castle Big Key
    case 0xEE:  // North Faron Woods Gate Key
    case 0xF3:  // Gate Keys
    case 0xF4:  // Ordon Pumpkin
    case 0xF5:  // Ordon Cheese
    case 0xF6:  // Snowpeak Ruins Bedroom Key
    case 0xF9:  // Goron Mines Key Shard
    case 0xFE:  // Faron Woods Coro Key
    // Portails
    case 0x14:  // Ordon Spring Portal
    case 0x15:  // South Faron Portal
    case 0x39:  // Upper Zora's River Portal
    case 0x3A:  // Castle Town Portal
    case 0x3B:  // Gerudo Desert Portal
    case 0x3C:  // North Faron Portal
    case 0x4D:  // Kakariko Gorge Portal
    case 0x4E:  // Kakariko Village Portal
    case 0x52:  // Death Mountain Portal
    case 0x57:  // Zora's Domain Portal
    case 0x8F:  // Lake Hylia Portal
    case 0xAE:  // Mirror Chamber Portal
    case 0xAF:  // Snowpeak Portal
    case 0xBF:  // Sacred Grove Portal
    case 0xE8:  // Bridge of Eldin Portal
        return true;
    default:
        return false;
    }
}

// Objet "Archipelago Item" : represente un objet qui appartient a un autre joueur (affiche
// comme un "Sol" en jeu). Numero tire des donnees du mod Archipelago.
constexpr int ARCHIPELAGO_ITEM = 0xDC;

// Tous les insectes (males et femelles) : du scarabee a l'ephemere.
bool is_insect_item(int item) {
    return item >= dItemNo_M_BEETLE_e && item <= dItemNo_F_MAYFLY_e;
}

// Petites cles de chaque donjon (numeros du randomizer : 0x85 a 0x8D).
bool is_small_key_item(int item) {
    return item >= 0x85 && item <= 0x8D;
}

// Quarts de coeur et conteneurs de coeur.
bool is_heart_item(int item) {
    return item == 0x21 || item == 0x22;
}

// Ames de Poe.
bool is_poe_item(int item) {
    return item == dItemNo_POU_SPIRIT_e;
}

// Choisit la teinte selon le contenu du coffre. Retourne nullptr si le coffre reste normal.
const Tint* tint_for_item(int item) {
    if (item == ARCHIPELAGO_ITEM) {
        return &TINT_WHITE;
    }
    if (is_insect_item(item)) {
        return &TINT_GREEN;
    }
    if (is_poe_item(item)) {
        return &TINT_PURPLE;
    }
    if (is_heart_item(item)) {
        return &TINT_RED;
    }
    if (is_small_key_item(item)) {
        return &TINT_BLUE;
    }
    if (is_important_item(item)) {
        return &TINT_GOLD;
    }
    return nullptr;
}

// Vrai si le contenu de ce coffre a ete modifie par un mod (donc en pratique : le randomizer).
// Sans randomizer, le jeu ne change jamais le contenu des coffres et rien n'est teinte.
bool chest_is_randomized(daTbox_c* chest) {
    // Cas 1 : l'objet du coffre n'est pas celui du jeu de base.
    if (chest->getItemNo() != chest->mOriginalItemNo) {
        return true;
    }

    // Cas 2 : le randomizer a remis le meme objet que dans le jeu de base. On demande au
    // service d'items si un mod a quand meme pris en charge cet emplacement ("chest:<niveau>:<n>").
    if (svc_item == nullptr || !SERVICE_HAS(svc_item, ItemService, resolve_check_full) ||
        svc_item->resolve_check_full == nullptr)
    {
        return false;
    }
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr) {
        return false;
    }
    char name[64];
    std::snprintf(name, sizeof(name), ITEM_CHECK_CHEST_PREFIX "%s:%d", stage, chest->getTboxNo());

    ItemCheckResolution resolution = {};
    resolution.item = chest->mOriginalItemNo;
    resolution.display_item = 0xFF;
    resolution.was_resolved = false;
    if (svc_item->resolve_check_full(mod_ctx, name, chest->mOriginalItemNo, &resolution) !=
        MOD_OK)
    {
        return false;
    }
    return resolution.was_resolved;
}

// Coffre en cours de dessin et sa teinte (nullptr si le coffre reste normal).
daTbox_c* g_drawingChest = nullptr;
const Tint* g_drawingTint = nullptr;

int clamp_int(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

DEFINE_HOOK(&daTbox_c::Draw, TboxDraw);
DEFINE_HOOK(static_cast<void (dScnKy_env_light_c::*)(J3DModelData*, dKy_tevstr_c*)>(
                &dScnKy_env_light_c::setLightTevColorType_MAJI),
    LightTevColorMaji);

HookAction on_tbox_draw_pre(ModContext*, void* args, void*, void*) {
    daTbox_c* chest = mods::arg<daTbox_c*>(args, 0);
    g_drawingTint = (chest != nullptr) ? tint_for_item(chest->getItemNo()) : nullptr;
    // Teinte uniquement si le coffre fait partie d'une partie randomisee.
    if (g_drawingTint != nullptr && !chest_is_randomized(chest)) {
        g_drawingTint = nullptr;
    }
    g_drawingChest = (g_drawingTint != nullptr) ? chest : nullptr;
    return HOOK_CONTINUE;
}

void on_tbox_draw_post(ModContext*, void*, void*, void*) {
    g_drawingChest = nullptr;
    g_drawingTint = nullptr;
}

HookAction on_light_color_pre(ModContext*, void* args, void*, void*) {
    if (g_drawingChest == nullptr || g_drawingTint == nullptr || g_drawingChest->mpModel == nullptr) {
        return HOOK_CONTINUE;
    }

    // args[0] = this (env light), args[1] = donnees du modele, args[2] = couleurs du coffre.
    J3DModelData* modelData = mods::arg<J3DModelData*>(args, 1);
    dKy_tevstr_c* tev = mods::arg<dKy_tevstr_c*>(args, 2);

    // Ne touche qu'au modele du coffre lui-meme (pas a l'effet lumineux qui partage les couleurs).
    if (tev != &g_drawingChest->tevStr || modelData != g_drawingChest->mpModel->getModelData()) {
        return HOOK_CONTINUE;
    }

    const Tint& t = *g_drawingTint;

    tev->AmbCol.r = static_cast<s16>(clamp_int(tev->AmbCol.r + t.ambR, 0, 255));
    tev->AmbCol.g = static_cast<s16>(clamp_int(tev->AmbCol.g + t.ambG, 0, 255));
    tev->AmbCol.b = static_cast<s16>(clamp_int(tev->AmbCol.b + t.ambB, 0, 255));

    tev->TevColor.r = static_cast<s16>(clamp_int(tev->TevColor.r + t.tevR, -1024, 1023));
    tev->TevColor.g = static_cast<s16>(clamp_int(tev->TevColor.g + t.tevG, -1024, 1023));
    tev->TevColor.b = static_cast<s16>(clamp_int(tev->TevColor.b + t.tevB, -1024, 1023));

    return HOOK_CONTINUE;
}

}  // namespace

extern "C" {

MOD_EXPORT ModResult mod_initialize(ModError* error) {
    if (mods::hook::add_pre<TboxDraw>(on_tbox_draw_pre) != MOD_OK ||
        mods::hook::add_post<TboxDraw>(on_tbox_draw_post) != MOD_OK)
    {
        return mods::set_error(error, MOD_ERROR, "chest_tint: impossible de hooker daTbox_c::Draw");
    }
    if (mods::hook::add_pre<LightTevColorMaji>(on_light_color_pre) != MOD_OK) {
        return mods::set_error(
            error, MOD_ERROR, "chest_tint: impossible de hooker setLightTevColorType_MAJI");
    }
    svc_log->info(mod_ctx, "chest_tint initialized");
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    g_drawingChest = nullptr;
    g_drawingTint = nullptr;
    svc_log->info(mod_ctx, "chest_tint unloaded");
    return MOD_OK;
}
}
