//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

#include "scoring.hpp"
#include <huggle_core/wikiedit.hpp>
#include <huggle_core/wikisite.hpp>
#include <huggle_core/wikipage.hpp>
#include <huggle_core/configuration.hpp>
#include <huggle_core/querypool.hpp>
#include <huggle_core/generic.hpp>
#include <QJsonDocument>
#include <QJsonObject>
#include <huggle_core/syslog.hpp>
#include <QTimer>

#define SCORING_TIMEOUT this->GetConfig("timeout", "7")

scoring::scoring()
{
    this->tm = new QTimer();
    connect(this->tm, SIGNAL(timeout()), this, SLOT(Refresh()));
    this->tm->start(HUGGLE_TIMER);
}

scoring::~scoring()
{
    this->tm->stop();
    delete this->tm;
}

bool scoring::Register()
{
    this->Init();
    return true;
}

bool scoring::IsWorking()
{
    return true;
}

void scoring::Hook_Shutdown()
{
    this->SetConfig("timeout", SCORING_TIMEOUT);
}

void scoring::Hook_MainWindowOnLoad(void *window)
{
    // Let's get the configuration
    foreach (Huggle::WikiSite *x, hcfg->Projects)
    {
        if (!Huggle::Generic::SafeBool(x->GetProjectConfig()->GetConfig("ores-supported", "false")))
        {
            Huggle::Syslog::HuggleLogs->WarningLog(x->Name + " is not supported by scoring extension");
            this->enabled.insert(x, false);
        } else if (!Huggle::Generic::SafeBool(x->GetProjectConfig()->GetConfig("ores-enabled", "false")))
        {
            this->enabled.insert(x, false);
        } else
        {
            this->enabled.insert(x, true);
            this->amplifiers.insert(x, x->GetProjectConfig()->GetConfig("ores-amplifier", "200").toDouble());
            this->server_url.insert(x, x->GetProjectConfig()->GetConfig("ores-urlv3", "https://ores.wikimedia.org/"));
        }
    }
}

bool scoring::Hook_EditIsReady(void *edit)
{
    if (this->Edits.contains(edit))
        return false;

    return true;
}

bool scoring::Hook_EditBeforeScore(void *edit)
{
    return true;
}

void scoring::Hook_EditBeforePostProcessing(void *edit)
{
    if (this->Edits.contains(edit))
    {
        HUGGLE_EXDEBUG1("edit " + QString::number((intptr_t) edit, 16) + " is already in list");
        return;
    }
    Huggle::WikiEdit *WikiEdit = (Huggle::WikiEdit*)edit;
    if (!this->enabled[WikiEdit->GetSite()])
        return;
    if (WikiEdit->RevID == WIKI_UNKNOWN_REVID)
        return;
    WikiEdit->IncRef();
    Huggle::Collectable_SmartPtr<Huggle::WebserverQuery> query = new Huggle::WebserverQuery();
    query->Timeout = SCORING_TIMEOUT.toInt();
    query->URL = this->GetServer(WikiEdit->GetSite()) + WikiEdit->GetSite()->Name + "?revids=" + QString::number(WikiEdit->RevID);
    query->Process();
    Huggle::QueryPool::HugglePool->AppendQuery(query);
    this->Edits.insert(edit, query);
    return;
}

void scoring::Hook_GoodEdit(void *edit)
{

}

double scoring::GetAmplifier(Huggle::WikiSite *site)
{
    if (!this->amplifiers.contains(site))
        return 200;
    return this->amplifiers[site];
}

bool scoring::compute_scores(long *final, QJsonObject score, Huggle::WikiEdit *wiki_edit)
{
    if (!score.contains("score") || !score["score"].toObject().contains("probability"))
        return false;

    QJsonObject probability = score["score"].toObject()["probability"].toObject();
    if (!probability.contains("false") || !probability.contains("true"))
        return false;

    // Math
    *final = (long)((probability["true"].toDouble() - probability["false"].toDouble()) * this->GetAmplifier(wiki_edit->GetSite()));
    return true;
}

void scoring::Refresh()
{
    foreach (void* edit, this->Edits.keys())
    {
        Huggle::Collectable_SmartPtr<Huggle::WebserverQuery> request = this->Edits[edit];
        Huggle::Collectable_SmartPtr<Huggle::WikiEdit> wiki_edit = (Huggle::WikiEdit*)edit;
        if (request->IsProcessed())
        {
            this->Edits.remove(edit);
            wiki_edit->DecRef();
            if (request->IsFailed())
            {
                Huggle::Syslog::HuggleLogs->ErrorLog("Scoring failed for edit " + wiki_edit->Page->PageName + ": " + request->GetFailureReason());
            } else
            {
                //HUGGLE_EXDEBUG(request->Result->Data, 3);
                QString project = wiki_edit->GetSite()->Name;
                QJsonDocument d = QJsonDocument::fromJson(request->Result->Data.toUtf8());
                QJsonObject score = d.object();
                QString revid = QString::number(wiki_edit->RevID);
                if (!score.contains(project))
                {
                    HUGGLE_EXDEBUG1("ORES score for edit " + revid + " on project " + wiki_edit->GetSite()->Name + " did not contain data, full source: " + request->Result->Data);
                    continue;
                }
                QJsonObject project_data = score[project].toObject();
                if (!project_data.contains("scores"))
                {
                    HUGGLE_EXDEBUG1("ORES score for edit " + revid + " on project " + wiki_edit->GetSite()->Name + " did not contain any scores");
                    HUGGLE_EXDEBUG("Debug data for ORES score: " + request->Result->Data, 4);
                    continue;
                }
                QJsonObject scores = project_data["scores"].toObject();
                if (!scores.contains(revid))
                {
                    HUGGLE_EXDEBUG1("ORES score for edit " + revid + " on project " + wiki_edit->GetSite()->Name + " did not contain scores for this revision");
                    HUGGLE_EXDEBUG("Debug data for ORES score: " + request->Result->Data, 4);
                    continue;
                }
                QJsonObject revision_data = scores[revid].toObject();
                long final;
                if (!revision_data.contains("damaging"))
                {
                    HUGGLE_EXDEBUG("Revision didn't contain damaging model, falling back to reverted model " + revid, 2);
                    if (!revision_data.contains("reverted"))
                    {
                        HUGGLE_EXDEBUG1("ORES score for edit " + revid + " on project " + wiki_edit->GetSite()->Name + " did not contain any useful model");
                        HUGGLE_EXDEBUG("Debug data for ORES score: " + request->Result->Data, 4);
                        continue;
                    }
                    if (!compute_scores(&final, revision_data["reverted"].toObject(), wiki_edit.GetPtr()))
                        continue;
                } else
                {
                    if (!compute_scores(&final, revision_data["damaging"].toObject(), wiki_edit.GetPtr()))
                        continue;
                }
                if (revision_data.contains("goodfaith"))
                {
                    long g_final;
                    if (!compute_scores(&g_final, revision_data["goodfaith"].toObject(), wiki_edit.GetPtr()))
                    {
                        HUGGLE_EXDEBUG("goodfaith is not available for " + revid, 3);
                    } else
                    {
                        wiki_edit->GoodfaithScore += g_final;
                        if (wiki_edit->MetaLabels.contains("ORES Goodfaith"))
                        {
                            // Now this is really bad - ores extension is loaded twice
                            HUGGLE_EXDEBUG1(revid + " on project " + wiki_edit->GetSite()->Name + " already contains Goodfaith meta-data, something is seriously broken!!");
                        } else
                        {
                            wiki_edit->MetaLabels.insert("ORES Goodfaith", QString::number(g_final));
                        }
                    }
                }

                wiki_edit->Score += final;
                if (!wiki_edit->MetaLabels.contains("ORES Score"))
                {
                    // Insert meta
                    wiki_edit->MetaLabels.insert("ORES Score", QString::number(final));
                }
            }
        }
    }
}

QString scoring::GetServer(Huggle::WikiSite *w)
{
    if (!this->server_url.contains(w))
        return "";

    // Get the cached project url
    return this->server_url[w] + "v3/scores/";
}

#if QT_VERSION < 0x050000
    Q_EXPORT_PLUGIN2("org.huggle.extension.qt", scoring)
#endif

