#include "race_scene.hpp"
#include "track_renderer.hpp"
#include "../ui/neon.hpp"
void DrawGridFloor(int slices,float spacing,Color color){float e=slices*spacing*.5f;for(int i=0;i<=slices;++i){float o=-e+i*spacing;DrawLine3D(Vector3{o,0,-e},Vector3{o,0,e},color);DrawLine3D(Vector3{-e,0,o},Vector3{e,0,o},color);}}
void DrawRaceTrackScene(const Track& track){for(std::vector<TrackPiece>::const_iterator p=track.Pieces().begin();p!=track.Pieces().end();++p)DrawTrackPieceSurface(*p,Fade(Neon::Panel,.96f),Neon::Cyan);if(track.HasStartFinish()){const TrackPiece* start=track.GetPiece(track.StartFinishPieceId());if(start!=0){const GridPosition pos=start->EntryConnector().position;DrawCube(Vector3{(float)pos.x,(float)pos.y+.18f,(float)pos.z},.95f,.10f,.95f,Neon::Green);}}}
